#include "airmon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_image_format.h"

extern const uint8_t index_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_end[] asm("_binary_index_html_gz_end");
static esp_err_t json_reply(httpd_req_t *r,cJSON *j) {
    char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!s)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    esp_err_t e=httpd_resp_send(r,s,HTTPD_RESP_USE_STRLEN);free(s);return e;
}
static esp_err_t error(httpd_req_t *r,const char *status,const char *message) {
    httpd_resp_set_status(r,status);cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"error",message);return json_reply(r,j);
}
static bool authenticated(httpd_req_t *r) {
    char header[80]={0};
    if(httpd_req_get_hdr_value_str(r,"Authorization",header,sizeof(header))!=ESP_OK || strncmp(header,"Bearer ",7) || !am_constant_time_equal(header+7,app.admin,33)) {
        httpd_resp_set_hdr(r,"WWW-Authenticate","Bearer");error(r,"401 Unauthorized","Administrator token required");return false;
    }
    /* No cross-origin grants. Only explicit bearer requests can mutate state. */
    return true;
}
static cJSON *body(httpd_req_t *r) {
    if(r->content_len<=0 || r->content_len>2048){error(r,"413 Content Too Large","JSON body must be 1–2048 bytes");return NULL;}
    char *b=calloc(1,r->content_len+1);if(!b){error(r,"500 Internal Server Error","Out of memory");return NULL;}
    size_t received=0;while(received<r->content_len){int n=httpd_req_recv(r,b+received,r->content_len-received);if(n<=0){free(b);error(r,"400 Bad Request","Incomplete request body");return NULL;}received+=n;}
    cJSON *j=cJSON_ParseWithLengthOpts(b,received+1,NULL,true);free(b);if(!j)error(r,"400 Bad Request","Invalid JSON");return j;
}
static esp_err_t readings(httpd_req_t *r) {
    am_reading values[AM_METRICS];am_lock();memcpy(values,app.readings,sizeof(values));am_unlock();
    cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"uptime_ms",am_now());
    uint32_t utc=am_utc();if(utc)cJSON_AddNumberToObject(j,"timestamp",utc);else cJSON_AddNullToObject(j,"timestamp");
    cJSON *metrics=cJSON_AddObjectToObject(j,"metrics");
    for(int i=0;i<AM_METRICS;i++) {
        am_reading *v=&values[i];cJSON *m=cJSON_AddObjectToObject(metrics,am_keys[i]);
        bool valid=am_reading_current(v,am_now(),i==AM_CO2?15000:5000);
        if(valid)cJSON_AddNumberToObject(m,"value",v->value);else cJSON_AddNullToObject(m,"value");
        cJSON_AddStringToObject(m,"unit",am_units[i]);
        cJSON_AddStringToObject(m,"status",am_state_names[v->state==AM_VALID&&!valid?AM_STALE:v->state]);
        if(v->at_ms)cJSON_AddNumberToObject(m,"age_ms",am_now()-v->at_ms);else cJSON_AddNullToObject(m,"age_ms");
    }return json_reply(r,j);
}
static esp_err_t status(httpd_req_t *r) {
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"version",AM_VERSION);cJSON_AddStringToObject(j,"device_id",app.id);
    cJSON_AddStringToObject(j,"board","E32R28T");cJSON_AddNumberToObject(j,"uptime_ms",am_now());cJSON_AddNumberToObject(j,"free_heap",esp_get_free_heap_size());
    cJSON_AddNumberToObject(j,"minimum_free_heap",esp_get_minimum_free_heap_size());
    am_lock();cJSON_AddBoolToObject(j,"wifi_connected",app.wifi_connected);cJSON_AddStringToObject(j,"ip",app.ip);
    cJSON_AddBoolToObject(j,"ap_active",app.ap_active);cJSON_AddBoolToObject(j,"mqtt_connected",app.mqtt_connected);
    cJSON_AddBoolToObject(j,"config_pending",app.config_pending);cJSON_AddStringToObject(j,"config_result",app.config_result);
    cJSON_AddBoolToObject(j,"touch_calibrated",app.touch_calibrated);cJSON_AddBoolToObject(j,"ota_busy",app.ota_busy);
    cJSON_AddNumberToObject(j,"history_points",app.history.count);
    cJSON *errors=cJSON_AddArrayToObject(j,"sensor_errors");for(int i=0;i<4;i++)cJSON_AddItemToArray(errors,cJSON_CreateNumber(app.sensor_errors[i]));am_unlock();
    return json_reply(r,j);
}
static esp_err_t history(httpd_req_t *r) {
    char query[100]={0},value[24];uint32_t before=UINT32_MAX,limit=180;
    size_t query_length=httpd_req_get_url_query_len(r);
    if(query_length) {
        if(query_length>=sizeof(query) || httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK)
            return error(r,"400 Bad Request","History query is too long or invalid");
        esp_err_t e=httpd_query_key_value(query,"before",value,sizeof(value));
        if(e!=ESP_ERR_NOT_FOUND && (e!=ESP_OK || !am_parse_u32(value,&before)))
            return error(r,"400 Bad Request","before must be unsigned decimal seconds within uint32 range");
        e=httpd_query_key_value(query,"limit",value,sizeof(value));
        if(e!=ESP_ERR_NOT_FOUND && (e!=ESP_OK || !am_parse_u32(value,&limit)))
            return error(r,"400 Bad Request","limit must be decimal digits from 1 to 180");
    }
    if(limit<1||limit>180)return error(r,"400 Bad Request","limit must be 1–180");
    am_history_point *page=malloc(limit*sizeof(*page));if(!page)return error(r,"503 Service Unavailable","Insufficient memory");
    unsigned count=0;am_lock();for(size_t i=app.history.count;i>0 && count<limit;i--) {const am_history_point *p=am_history_at(&app.history,i-1);if(p->uptime_s<before)page[count++]=*p;}am_unlock();
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    const char *prefix="{\"resolution_seconds\":60,\"persistent\":false,\"points\":[";
    esp_err_t e=httpd_resp_send_chunk(r,prefix,strlen(prefix));
    for(unsigned i=count;i>0 && e==ESP_OK;i--) {
        am_history_point *p=&page[i-1];cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"uptime_s",p->uptime_s);
        if(p->utc_s)cJSON_AddNumberToObject(j,"timestamp",p->utc_s);else cJSON_AddNullToObject(j,"timestamp");
        for(int m=0;m<AM_METRICS;m++){if(p->values[m]!=AM_INVALID)cJSON_AddNumberToObject(j,am_keys[m],(double)p->values[m]/am_scales[m]);else cJSON_AddNullToObject(j,am_keys[m]);}
        char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
        if(i<count)e=httpd_resp_send_chunk(r,",",1);
        if(s&&e==ESP_OK)e=httpd_resp_send_chunk(r,s,strlen(s));else if(!s)e=ESP_ERR_NO_MEM;free(s);
    }
    char end[64];if(count)snprintf(end,sizeof(end),"],\"next_before\":%lu}",(unsigned long)page[count-1].uptime_s);else strlcpy(end,"],\"next_before\":null}",sizeof(end));free(page);
    if(e==ESP_OK)e=httpd_resp_send_chunk(r,end,strlen(end));
    if(e==ESP_OK)e=httpd_resp_send_chunk(r,NULL,0);
    return e;
}
static esp_err_t get_config(httpd_req_t *r){return json_reply(r,am_config_json());}
static esp_err_t put_config(httpd_req_t *r) {
    if(!authenticated(r))return ESP_OK;
    cJSON *j=body(r);if(!j)return ESP_OK;
    am_config candidate;char message[128];bool ok=am_config_parse(j,&candidate,message,sizeof(message));cJSON_Delete(j);
    if(!ok)return error(r,"400 Bad Request",message);
    if(!am_network_configure(&candidate))return error(r,"409 Conflict","Another settings change or firmware update is active");
    httpd_resp_set_status(r,"202 Accepted");j=cJSON_CreateObject();cJSON_AddStringToObject(j,"status","pending");return json_reply(r,j);
}
static esp_err_t calibration(httpd_req_t *r) {
    if(!authenticated(r))return ESP_OK;
    cJSON *j=body(r);if(!j)return ESP_OK;
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,"reference_ppm");
    bool ok=cJSON_IsNumber(v)&&v->valuedouble==v->valueint&&v->valueint>=400&&v->valueint<=2000&&am_sensors_calibrate(v->valueint);cJSON_Delete(j);
    if(!ok)return error(r,"409 Conflict","CO2 must be valid and run for 3 minutes; reference must be 400–2000 ppm");
    httpd_resp_set_status(r,"202 Accepted");j=cJSON_CreateObject();cJSON_AddStringToObject(j,"status","calibration_pending");return json_reply(r,j);
}
static esp_err_t recalibrate(httpd_req_t *r) {if(!authenticated(r))return ESP_OK;am_display_recalibrate();return httpd_resp_sendstr(r,"Touch calibration started on the display");}
static void reboot(void *unused){(void)unused;vTaskDelay(pdMS_TO_TICKS(1200));esp_restart();}
static esp_err_t firmware(httpd_req_t *r) {
    if(!authenticated(r))return ESP_OK;
    am_lock();bool busy=app.config_pending||app.ota_busy;if(!busy)app.ota_busy=true;am_unlock();
    if(busy)return error(r,"409 Conflict","Device is busy");
    const esp_partition_t *p=esp_ota_get_next_update_partition(NULL);esp_ota_handle_t handle=0;esp_err_t e=ESP_FAIL;
    if(!p||r->content_len<sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t)||r->content_len>p->size)goto failed;
    uint8_t buffer[2048];size_t got=0;
    /* Accumulate enough header bytes even when TCP splits the first packet. */
    while(got<sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t)) {
        int n=httpd_req_recv(r,(char*)buffer+got,sizeof(buffer)-got);if(n<=0)goto failed;got+=n;
    }
    esp_image_header_t image;memcpy(&image,buffer,sizeof(image));
    esp_app_desc_t desc;memcpy(&desc,buffer+sizeof(image)+sizeof(esp_image_segment_header_t),sizeof(desc));
    if(image.magic!=ESP_IMAGE_HEADER_MAGIC || image.chip_id!=ESP_CHIP_ID_ESP32 || desc.magic_word!=ESP_APP_DESC_MAGIC_WORD || strncmp(desc.project_name,"airmon",sizeof(desc.project_name)))goto failed;
    e=esp_ota_begin(p,r->content_len,&handle);if(e!=ESP_OK)goto failed;
    e=esp_ota_write(handle,buffer,got);if(e!=ESP_OK)goto failed;
    size_t total=got;
    while(total<r->content_len) {
        size_t want=r->content_len-total;if(want>sizeof(buffer))want=sizeof(buffer);
        int n=httpd_req_recv(r,(char*)buffer,want);if(n<=0)goto failed;
        e=esp_ota_write(handle,buffer,n);if(e!=ESP_OK)goto failed;total+=n;
    }
    e=esp_ota_end(handle);handle=0;if(e!=ESP_OK)goto failed;
    e=esp_ota_set_boot_partition(p);if(e!=ESP_OK)goto failed;
    httpd_resp_sendstr(r,"Firmware validated. Rebooting.");xTaskCreate(reboot,"ota_reboot",2048,NULL,4,NULL);return ESP_OK;
failed:
    if(handle)esp_ota_abort(handle);
    am_lock();app.ota_busy=false;am_unlock();
    return error(r,"400 Bad Request","Upload failed or image is incompatible; current firmware retained");
}
static esp_err_t root(httpd_req_t *r) {
    if(!strncmp(r->uri,"/api/",5))return error(r,"404 Not Found","Unknown API endpoint");
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Content-Encoding","gzip");httpd_resp_set_hdr(r,"Cache-Control","no-cache");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");return httpd_resp_send(r,(const char*)index_start,index_end-index_start);
}
void am_web_start(void) {
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=10240;cfg.max_uri_handlers=12;cfg.uri_match_fn=httpd_uri_match_wildcard;cfg.lru_purge_enable=true;cfg.recv_wait_timeout=10;
    httpd_handle_t server;ESP_ERROR_CHECK(httpd_start(&server,&cfg));
    const httpd_uri_t routes[]={
        {.uri="/api/v1/readings",.method=HTTP_GET,.handler=readings},
        {.uri="/api/v1/status",.method=HTTP_GET,.handler=status},
        {.uri="/api/v1/history",.method=HTTP_GET,.handler=history},
        {.uri="/api/v1/config",.method=HTTP_GET,.handler=get_config},
        {.uri="/api/v1/config",.method=HTTP_PUT,.handler=put_config},
        {.uri="/api/v1/calibration/co2",.method=HTTP_POST,.handler=calibration},
        {.uri="/api/v1/calibration/touch",.method=HTTP_POST,.handler=recalibrate},
        {.uri="/api/v1/firmware",.method=HTTP_POST,.handler=firmware},
        {.uri="/*",.method=HTTP_GET,.handler=root}};
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}

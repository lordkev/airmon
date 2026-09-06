#include "airmon.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "esp_mac.h"
#include "nvs.h"

static void random_hex(char *out,size_t bytes) {
    static const char hex[]="0123456789abcdef";
    for(size_t i=0;i<bytes;i++) {uint8_t r=(uint8_t)esp_random();out[i*2]=hex[r>>4];out[i*2+1]=hex[r&15];}
    out[bytes*2]=0;
}
void am_config_init(void) {
    uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    snprintf(app.id,sizeof(app.id),"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    snprintf(app.ap_ssid,sizeof(app.ap_ssid),"AirMon-%.6s",app.id+6);
    app.config=(am_config){.version=1,.brightness=30,.dim_seconds=120};
    snprintf(app.config.name,sizeof(app.config.name),"AirMon %.6s",app.id+6);
    nvs_handle_t n;ESP_ERROR_CHECK(nvs_open("airmon",NVS_READWRITE,&n));
    size_t size=sizeof(am_config);am_config stored;
    esp_err_t e=nvs_get_blob(n,"config",&stored,&size);
    if(e==ESP_OK && size==sizeof(stored) && stored.version==1) app.config=stored;
    size=sizeof(app.admin);
    if(nvs_get_str(n,"admin",app.admin,&size)!=ESP_OK) {random_hex(app.admin,16);ESP_ERROR_CHECK(nvs_set_str(n,"admin",app.admin));}
    size=sizeof(app.ap_password);
    if(nvs_get_str(n,"ap_pass",app.ap_password,&size)!=ESP_OK) {random_hex(app.ap_password,8);ESP_ERROR_CHECK(nvs_set_str(n,"ap_pass",app.ap_password));}
    size=sizeof(app.touch_affine);
    app.touch_calibrated=nvs_get_blob(n,"touch",app.touch_affine,&size)==ESP_OK && size==sizeof(app.touch_affine);
    ESP_ERROR_CHECK(nvs_commit(n));nvs_close(n);
}
void am_config_get(am_config *c) {am_lock();*c=app.config;am_unlock();}
esp_err_t am_config_save(const am_config *c) {
    nvs_handle_t n;esp_err_t e=nvs_open("airmon",NVS_READWRITE,&n);if(e!=ESP_OK)return e;
    e=nvs_set_blob(n,"config",c,sizeof(*c));if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);
    if(e==ESP_OK){am_lock();app.config=*c;am_unlock();}return e;
}
void am_touch_save(const float a[6]) {
    nvs_handle_t n;if(nvs_open("airmon",NVS_READWRITE,&n)!=ESP_OK)return;
    if(nvs_set_blob(n,"touch",a,6*sizeof(float))==ESP_OK && nvs_commit(n)==ESP_OK) {
        am_lock();memcpy(app.touch_affine,a,sizeof(app.touch_affine));app.touch_calibrated=true;am_unlock();
    }nvs_close(n);
}
cJSON *am_config_json(void) {
    am_config c;am_config_get(&c);cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"name",c.name);cJSON_AddStringToObject(j,"ssid",c.ssid);
    cJSON_AddNumberToObject(j,"brightness",c.brightness);cJSON_AddNumberToObject(j,"dim_seconds",c.dim_seconds);
    cJSON_AddNumberToObject(j,"temperature_offset",c.temperature_offset);cJSON_AddNumberToObject(j,"altitude_m",c.altitude_m);
    cJSON_AddBoolToObject(j,"co2_asc",c.co2_asc);cJSON_AddBoolToObject(j,"mqtt_enabled",c.mqtt_enabled);
    cJSON_AddStringToObject(j,"mqtt_uri",c.mqtt_uri);cJSON_AddStringToObject(j,"mqtt_user",c.mqtt_user);
    cJSON_AddBoolToObject(j,"wifi_password_set",c.password[0]);cJSON_AddBoolToObject(j,"mqtt_password_set",c.mqtt_password[0]);return j;
}
bool am_config_parse(const cJSON *j,am_config *c,char *err,size_t length) {
    if(!cJSON_IsObject(j)){snprintf(err,length,"Expected a JSON object");return false;}
    am_config_get(c);
    const cJSON *v;
    cJSON_ArrayForEach(v,j) {
        const char *key=v->string;char *dest=NULL;size_t cap=0;
        if(!strcmp(key,"name")){dest=c->name;cap=sizeof(c->name);}
        else if(!strcmp(key,"ssid")){dest=c->ssid;cap=sizeof(c->ssid);}
        else if(!strcmp(key,"password")){dest=c->password;cap=sizeof(c->password);}
        else if(!strcmp(key,"mqtt_uri")){dest=c->mqtt_uri;cap=sizeof(c->mqtt_uri);}
        else if(!strcmp(key,"mqtt_user")){dest=c->mqtt_user;cap=sizeof(c->mqtt_user);}
        else if(!strcmp(key,"mqtt_password")){dest=c->mqtt_password;cap=sizeof(c->mqtt_password);}
        if(dest) {
            if(!cJSON_IsString(v)||strlen(v->valuestring)>=cap)goto invalid;
            for(const unsigned char *p=(void*)v->valuestring;*p;p++)if(*p<32)goto invalid;
            strcpy(dest,v->valuestring);continue;
        }
        if(!strcmp(key,"co2_asc")||!strcmp(key,"mqtt_enabled")) {
            if(!cJSON_IsBool(v))goto invalid;
            if(!strcmp(key,"co2_asc"))c->co2_asc=cJSON_IsTrue(v);else c->mqtt_enabled=cJSON_IsTrue(v);continue;
        }
        if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))goto invalid;
        double d=v->valuedouble;
        if(!strcmp(key,"temperature_offset")){if(d < -10 || d>10)goto invalid;c->temperature_offset=d;}
        else if(!strcmp(key,"brightness")){if(d<5||d>100||floor(d)!=d)goto invalid;c->brightness=d;}
        else if(!strcmp(key,"dim_seconds")){if(d<0||d>3600||floor(d)!=d)goto invalid;c->dim_seconds=d;}
        else if(!strcmp(key,"altitude_m")){if(d<0||d>3000||floor(d)!=d)goto invalid;c->altitude_m=d;}
        else goto invalid;
    }
    if(!c->name[0] || !c->ssid[0]){snprintf(err,length,"Device name and Wi-Fi SSID are required");return false;}
    if(c->password[0] && (strlen(c->password)<8 || strlen(c->password)>63)) {snprintf(err,length,"Wi-Fi password must be empty for an open network or 8–63 characters");return false;}
    if(c->mqtt_enabled && strncmp(c->mqtt_uri,"mqtt://",7) && strncmp(c->mqtt_uri,"mqtts://",8)) {snprintf(err,length,"MQTT URI must start with mqtt:// or mqtts://");return false;}
    if(strchr(c->mqtt_uri,'@')) {snprintf(err,length,"Use the separate MQTT username and password fields; URI credentials are not allowed");return false;}
    if(c->mqtt_enabled && (!strstr(c->mqtt_uri,"://")[3] || strchr(c->mqtt_uri,' '))) {snprintf(err,length,"MQTT URI needs a broker hostname or IP address");return false;}
    return true;
invalid: snprintf(err,length,"Invalid or unknown setting: %s",v->string);return false;
}

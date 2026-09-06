#include "airmon.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_crt_bundle.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "lwip/inet.h"
#include "mdns.h"
#include "mqtt_client.h"

static EventGroupHandle_t events;
static QueueHandle_t updates;
static esp_mqtt_client_handle_t mqtt;
static atomic_bool reconnect_enabled=true;
static uint64_t close_ap_at;
static const char *const classes[AM_METRICS]={"temperature","humidity","pm1","pm25","pm10","carbon_dioxide",NULL,NULL};

static void publish_discovery(void) {
    am_config cfg;am_config_get(&cfg);
    for(int i=0;i<AM_METRICS;i++) {
        char topic[160],state[100],available[120],unique[80],value[80];
        snprintf(topic,sizeof(topic),"homeassistant/sensor/airmon_%s/%s/config",app.id,am_keys[i]);
        snprintf(state,sizeof(state),"airmon/%s/state",app.id);
        snprintf(unique,sizeof(unique),"airmon_%s_%s",app.id,am_keys[i]);
        snprintf(value,sizeof(value),"{{ value_json.%s }}",am_keys[i]);
        cJSON *j=cJSON_CreateObject();
        cJSON_AddStringToObject(j,"name",am_keys[i]);cJSON_AddStringToObject(j,"unique_id",unique);
        cJSON_AddStringToObject(j,"state_topic",state);cJSON_AddStringToObject(j,"value_template",value);
        cJSON_AddStringToObject(j,"unit_of_measurement",am_units[i]);cJSON_AddStringToObject(j,"state_class","measurement");
        cJSON_AddNumberToObject(j,"expire_after",35);
        if(classes[i])cJSON_AddStringToObject(j,"device_class",classes[i]);
        cJSON *availability=cJSON_AddArrayToObject(j,"availability");
        snprintf(available,sizeof(available),"airmon/%s/availability",app.id);
        cJSON *a=cJSON_CreateObject();cJSON_AddStringToObject(a,"topic",available);cJSON_AddItemToArray(availability,a);
        snprintf(available,sizeof(available),"airmon/%s/availability/%s",app.id,am_keys[i]);
        a=cJSON_CreateObject();cJSON_AddStringToObject(a,"topic",available);cJSON_AddItemToArray(availability,a);
        cJSON_AddStringToObject(j,"availability_mode","all");
        cJSON *device=cJSON_AddObjectToObject(j,"device"),*ids=cJSON_AddArrayToObject(device,"identifiers");
        snprintf(unique,sizeof(unique),"airmon_%s",app.id);cJSON_AddItemToArray(ids,cJSON_CreateString(unique));
        cJSON_AddStringToObject(device,"name",cfg.name);cJSON_AddStringToObject(device,"manufacturer","AirMon DIY");
        cJSON_AddStringToObject(device,"model","Keyestudio E32R28T + AirMon carrier");cJSON_AddStringToObject(device,"sw_version",AM_VERSION);
        char *data=cJSON_PrintUnformatted(j);if(data){esp_mqtt_client_enqueue(mqtt,topic,data,0,1,true,true);free(data);}cJSON_Delete(j);
    }
}
static void publish_state(void) {
    am_reading r[AM_METRICS];am_lock();memcpy(r,app.readings,sizeof(r));am_unlock();
    cJSON *j=cJSON_CreateObject();char topic[120];
    for(int i=0;i<AM_METRICS;i++) {
        bool valid=am_reading_current(&r[i],am_now(),i==AM_CO2?15000:5000);
        if(valid)cJSON_AddNumberToObject(j,am_keys[i],r[i].value);else cJSON_AddNullToObject(j,am_keys[i]);
        snprintf(topic,sizeof(topic),"airmon/%s/availability/%s",app.id,am_keys[i]);
        esp_mqtt_client_enqueue(mqtt,topic,valid?"online":"offline",0,1,true,true);
    }
    snprintf(topic,sizeof(topic),"airmon/%s/state",app.id);char *data=cJSON_PrintUnformatted(j);
    if(data){esp_mqtt_client_enqueue(mqtt,topic,data,0,0,false,true);free(data);}cJSON_Delete(j);
    snprintf(topic,sizeof(topic),"airmon/%s/availability",app.id);esp_mqtt_client_enqueue(mqtt,topic,"online",0,1,true,true);
}
static void mqtt_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;(void)base;esp_mqtt_event_handle_t e=data;
    if(id==MQTT_EVENT_CONNECTED) {
        am_lock();app.mqtt_connected=true;am_unlock();
        esp_mqtt_client_subscribe(mqtt,"homeassistant/status",1);publish_discovery();publish_state();
    }else if(id==MQTT_EVENT_DISCONNECTED){am_lock();app.mqtt_connected=false;am_unlock();}
    else if(id==MQTT_EVENT_DATA && e->topic_len==20 && !memcmp(e->topic,"homeassistant/status",20) && e->data_len==6 && !memcmp(e->data,"online",6)) {publish_discovery();publish_state();}
}
static void mqtt_restart(void) {
    if(mqtt){esp_mqtt_client_stop(mqtt);esp_mqtt_client_destroy(mqtt);mqtt=NULL;}
    am_lock();app.mqtt_connected=false;am_unlock();am_config c;am_config_get(&c);if(!c.mqtt_enabled)return;
    char topic[96],client[40];snprintf(topic,sizeof(topic),"airmon/%s/availability",app.id);snprintf(client,sizeof(client),"airmon_%s",app.id);
    esp_mqtt_client_config_t m={.broker.address.uri=c.mqtt_uri,.broker.verification.crt_bundle_attach=esp_crt_bundle_attach,
        .credentials.client_id=client,.credentials.username=c.mqtt_user[0]?c.mqtt_user:NULL,.credentials.authentication.password=c.mqtt_password[0]?c.mqtt_password:NULL,
        .session.last_will={.topic=topic,.msg="offline",.qos=1,.retain=true},.session.keepalive=30,.network.reconnect_timeout_ms=5000,.outbox.limit=8192};
    mqtt=esp_mqtt_client_init(&m);if(mqtt){esp_mqtt_client_register_event(mqtt,ESP_EVENT_ANY_ID,mqtt_event,NULL);esp_mqtt_client_start(mqtt);}
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) {am_config c;am_config_get(&c);if(c.ssid[0])esp_wifi_connect();}
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(events,BIT0);am_lock();app.wifi_connected=false;app.ip[0]=0;am_unlock();
        if(reconnect_enabled)esp_wifi_connect();
    }
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e=data;am_lock();app.wifi_connected=true;snprintf(app.ip,sizeof(app.ip),IPSTR,IP2STR(&e->ip_info.ip));am_unlock();xEventGroupSetBits(events,BIT0);
    }
}
static void apply_wifi(const am_config *c) {
    reconnect_enabled=false;esp_wifi_disconnect();xEventGroupClearBits(events,BIT0);
    wifi_config_t w={0};memcpy(w.sta.ssid,c->ssid,strlen(c->ssid));strlcpy((char*)w.sta.password,c->password,sizeof(w.sta.password));
    w.sta.pmf_cfg.capable=true;w.sta.pmf_cfg.required=false;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));reconnect_enabled=true;esp_wifi_connect();
}
void am_network_setup(void) {
    wifi_config_t w={0};strlcpy((char*)w.ap.ssid,app.ap_ssid,sizeof(w.ap.ssid));strlcpy((char*)w.ap.password,app.ap_password,sizeof(w.ap.password));
    w.ap.ssid_len=strlen(app.ap_ssid);w.ap.authmode=WIFI_AUTH_WPA2_PSK;w.ap.max_connection=3;w.ap.channel=1;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&w));
    am_lock();app.ap_active=true;close_ap_at=0;am_unlock();
}
bool am_network_configure(const am_config *c) {
    am_lock();if(app.config_pending || app.ota_busy){am_unlock();return false;}app.config_pending=true;strlcpy(app.config_result,"Applying settings",sizeof(app.config_result));am_unlock();
    if(xQueueSend(updates,c,0)!=pdTRUE){am_lock();app.config_pending=false;am_unlock();return false;}return true;
}
static void worker(void *unused) {
    (void)unused;am_config candidate;uint64_t next_publish=0;
    for(;;) {
        if(xQueueReceive(updates,&candidate,pdMS_TO_TICKS(1000))==pdTRUE) {
            am_config old;am_config_get(&old);
            bool change=strcmp(old.ssid,candidate.ssid)||strcmp(old.password,candidate.password);
            bool connected=true;
            if(change){apply_wifi(&candidate);connected=(xEventGroupWaitBits(events,BIT0,pdFALSE,pdTRUE,pdMS_TO_TICKS(40000))&BIT0)!=0;}
            bool saved=connected && am_config_save(&candidate)==ESP_OK;
            if(saved){mqtt_restart();am_lock();strlcpy(app.config_result,"Settings saved",sizeof(app.config_result));if(app.ap_active&&app.wifi_connected)close_ap_at=am_now()+60000;am_unlock();}
            else {
                if(change && old.ssid[0])apply_wifi(&old);
                else if(change){reconnect_enabled=false;esp_wifi_disconnect();}
                am_lock();strlcpy(app.config_result,connected?"Storage error: settings not saved":"Wi-Fi failed: previous settings restored",sizeof(app.config_result));am_unlock();
            }
            am_lock();app.config_pending=false;am_unlock();
        }
        am_lock();bool close=close_ap_at && am_now()>close_ap_at;bool online=app.mqtt_connected;am_unlock();
        if(close){esp_wifi_set_mode(WIFI_MODE_STA);am_lock();app.ap_active=false;close_ap_at=0;am_unlock();}
        if(mqtt && online && am_now()>next_publish){publish_state();next_publish=am_now()+10000;}
    }
}
static void dns_task(void *unused) {
    (void)unused;int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(fd<0){vTaskDelete(NULL);return;}
    struct sockaddr_in local={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=INADDR_ANY};
    if(bind(fd,(void*)&local,sizeof(local))<0){close(fd);vTaskDelete(NULL);return;}
    for(;;){uint8_t b[512];struct sockaddr_in peer;socklen_t len=sizeof(peer);int n=recvfrom(fd,b,sizeof(b)-16,0,(void*)&peer,&len);
        am_lock();bool active=app.ap_active;am_unlock();
        if(!active || n<17 || b[4]!=0 || b[5]!=1 || (b[2]&0x80))continue;
        size_t p=12;while(p<(size_t)n && b[p] && b[p]<=63){p+=1+b[p];}
        if(p+5>(size_t)n || b[p]!=0)continue;
        p++;
        if(b[p]!=0 || b[p+1]!=1 || b[p+2]!=0 || b[p+3]!=1)continue;
        p+=4;
        b[2]=0x81;b[3]=0x80;b[6]=0;b[7]=1;b[8]=b[9]=b[10]=b[11]=0;
        const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};memcpy(b+p,answer,sizeof(answer));
        sendto(fd,b,p+sizeof(answer),0,(void*)&peer,len);
    }
}
void am_network_start(void) {
    events=xEventGroupCreate();updates=xQueueCreate(1,sizeof(am_config));configASSERT(events&&updates);
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));am_config c;am_config_get(&c);
    if(!c.ssid[0])am_network_setup();
    else {wifi_config_t w={0};memcpy(w.sta.ssid,c.ssid,strlen(c.ssid));strlcpy((char*)w.sta.password,c.password,sizeof(w.sta.password));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));}
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(mdns_init());char hostname[32];snprintf(hostname,sizeof(hostname),"airmon-%.6s",app.id+6);mdns_hostname_set(hostname);mdns_instance_name_set("AirMon");mdns_service_add(NULL,"_http","_tcp",80,NULL,0);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();
    mqtt_restart();configASSERT(xTaskCreate(worker,"network",6144,NULL,4,NULL)==pdPASS);configASSERT(xTaskCreate(dns_task,"captive_dns",3072,NULL,3,NULL)==pdPASS);
}

/* Tests production config.c against a transactional, in-memory NVS fake.
   This validates parser/persistence logic, not actual flash or FreeRTOS locks. */
#include "airmon.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
am_app app;
typedef struct {char key[16];unsigned char data[1024];size_t len;} record;
static record disk[8],staged[8];static bool fail_commit;
uint32_t esp_random(void){static uint32_t n=123;n=n*1664525u+1013904223u;return n;}
esp_err_t esp_read_mac(uint8_t *mac,int type){(void)type;const uint8_t fixed[]={0xaa,0xbb,0xcc,0x11,0x22,0x33};memcpy(mac,fixed,6);return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *n){(void)mode;assert(!strcmp(name,"airmon"));memcpy(staged,disk,sizeof(disk));*n=1;return ESP_OK;}
void nvs_close(nvs_handle_t n){(void)n;}
esp_err_t nvs_commit(nvs_handle_t n){(void)n;if(fail_commit)return ESP_FAIL;memcpy(disk,staged,sizeof(disk));return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t n,const char *key,void *out,size_t *size){(void)n;for(int i=0;i<8;i++)if(!strcmp(disk[i].key,key)){if(*size<disk[i].len)return ESP_FAIL;*size=disk[i].len;memcpy(out,disk[i].data,*size);return ESP_OK;}return ESP_FAIL;}
esp_err_t nvs_set_blob(nvs_handle_t n,const char *key,const void *value,size_t len){(void)n;assert(len<=1024);for(int i=0;i<8;i++)if(!staged[i].key[0]||!strcmp(staged[i].key,key)){strcpy(staged[i].key,key);memcpy(staged[i].data,value,len);staged[i].len=len;return ESP_OK;}return ESP_FAIL;}
esp_err_t nvs_get_str(nvs_handle_t n,const char *key,char *out,size_t *size){return nvs_get_blob(n,key,out,size);}
esp_err_t nvs_set_str(nvs_handle_t n,const char *key,const char *value){return nvs_set_blob(n,key,value,strlen(value)+1);}
static bool parse(const char *s,am_config *c){cJSON *j=cJSON_Parse(s);assert(j);char error[160];bool ok=am_config_parse(j,c,error,sizeof(error));cJSON_Delete(j);return ok;}
int main(void){
 am_config_init();assert(app.config.brightness==30&&!app.config.co2_asc&&app.config.dim_seconds==120);
 assert(strlen(app.admin)==32&&strlen(app.ap_password)==16);
 char admin[33],pass[17];strcpy(admin,app.admin);strcpy(pass,app.ap_password);
 am_config c;assert(!parse("{\"brightness\":50}",&c));
 assert(parse("{\"ssid\":\"12345678901234567890123456789012\",\"password\":\"secretpass\"}",&c));
 assert(strlen(c.ssid)==32);assert(am_config_save(&c)==ESP_OK);
 memset(&app,0,sizeof(app));am_config_init();assert(!strcmp(app.admin,admin)&&!strcmp(app.ap_password,pass));assert(!strcmp(app.config.password,"secretpass"));
 assert(parse("{\"name\":\"Bedroom\",\"brightness\":5,\"dim_seconds\":0,\"temperature_offset\":-10,\"altitude_m\":3000}",&c));
 assert(!strcmp(c.password,"secretpass"));assert(am_config_save(&c)==ESP_OK);
 const char *bad[]={"[]","{\"unknown\":1}","{\"brightness\":4}","{\"brightness\":101}","{\"brightness\":30.5}","{\"dim_seconds\":3601}","{\"co2_asc\":1}","{\"temperature_offset\":11}","{\"ssid\":\"\"}","{\"name\":\"\"}","{\"password\":\"short\"}","{\"mqtt_enabled\":true,\"mqtt_uri\":\"https://broker\"}","{\"mqtt_enabled\":true,\"mqtt_uri\":\"mqtt://\"}","{\"mqtt_uri\":\"mqtt://user:secret@broker\"}","{\"name\":\"bad\\nname\"}","{\"wifi_password_set\":true}","{\"ssid\":\"123456789012345678901234567890123\"}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!parse(bad[i],&c));
 assert(parse("{\"mqtt_enabled\":true,\"mqtt_uri\":\"mqtts://broker.example.com:8883\",\"mqtt_password\":\"brokersecret\"}",&c));assert(am_config_save(&c)==ESP_OK);
 cJSON *redacted=am_config_json();assert(!cJSON_GetObjectItem(redacted,"password")&&!cJSON_GetObjectItem(redacted,"mqtt_password")&&!cJSON_GetObjectItem(redacted,"admin"));assert(cJSON_IsTrue(cJSON_GetObjectItem(redacted,"mqtt_password_set")));cJSON_Delete(redacted);
 assert(parse("{\"password\":\"\",\"mqtt_password\":\"\"}",&c));assert(!c.password[0]&&!c.mqtt_password[0]);
 fail_commit=true;assert(am_config_save(&c)!=ESP_OK);assert(!strcmp(app.config.password,"secretpass"));fail_commit=false;
 memset(&app,0,sizeof(app));am_config_init();assert(!strcmp(app.config.password,"secretpass"));
 float affine[]={1,0,0,0,1,0};am_touch_save(affine);memset(&app,0,sizeof(app));am_config_init();assert(app.touch_calibrated&&!memcmp(affine,app.touch_affine,sizeof(affine)));
 puts("PASS: configuration boundaries, redaction, credential/touch persistence, failed-commit preservation");
}

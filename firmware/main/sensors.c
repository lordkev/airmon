#include "airmon.h"
#include <math.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "freertos/queue.h"
#include "sensirion_gas_index_algorithm.h"

typedef struct {uint8_t address; i2c_master_dev_handle_t dev;bool online,ever;unsigned failures;uint64_t since;} sensor;
static sensor devices[4]={{.address=0x44},{.address=0x12},{.address=0x62},{.address=0x59}};
static i2c_master_bus_handle_t bus;
static QueueHandle_t calibrations;
static GasIndexAlgorithmParams voc,nox;
static const unsigned first[4]={AM_TEMP,AM_PM1,AM_CO2,AM_VOC},last[4]={AM_RH,AM_PM10,AM_CO2,AM_NOX};
static uint64_t co2_started;

static bool command(sensor *s,uint16_t command,const uint16_t *args,size_t count,uint16_t *out,size_t words,unsigned wait) {
    uint8_t b[20]={(uint8_t)(command>>8),(uint8_t)command};
    for(size_t i=0;i<count;i++){b[2+i*3]=args[i]>>8;b[3+i*3]=args[i];b[4+i*3]=am_crc8(b+2+i*3,2);}
    if(i2c_master_transmit(s->dev,b,2+count*3,100)!=ESP_OK)return false;
    if(wait)vTaskDelay(pdMS_TO_TICKS(wait));
    if(!words)return true;
    return i2c_master_receive(s->dev,b,words*3,100)==ESP_OK && am_sensirion_words(b,words*3,out);
}
static void set_state(unsigned id,am_state state) {
    am_lock();for(unsigned m=first[id];m<=last[id];m++)app.readings[m].state=state;am_unlock();
}
static void publish(unsigned metric,float value,am_state state) {
    am_lock();app.readings[metric]=(am_reading){.value=value,.at_ms=am_now(),.state=state};am_unlock();
}
static bool start_co2(sensor *s,const am_config *c) {
    if(!command(s,0x3f86,NULL,0,NULL,0,500))return false;
    uint16_t asc=c->co2_asc,alt=c->altitude_m;
    if(!command(s,0x2416,&asc,1,NULL,0,2)||!command(s,0x2427,&alt,1,NULL,0,2)||!command(s,0x21b1,NULL,0,NULL,0,1))return false;
    am_lock();co2_started=am_now();am_unlock();return true;
}
static void discover(unsigned i,const am_config *c) {
    sensor *s=&devices[i];
    if(i2c_master_probe(bus,s->address,100)!=ESP_OK){set_state(i,s->ever?AM_FAILED:AM_ABSENT);return;}
    bool ok=true;
    if(i==2)ok=start_co2(s,c);
    if(i==3) {
        uint16_t result=0;ok=command(s,0x280e,NULL,0,&result,1,320)&&result==0xd400;
        GasIndexAlgorithm_init(&voc,GasIndexAlgorithm_ALGORITHM_TYPE_VOC);
        GasIndexAlgorithm_init(&nox,GasIndexAlgorithm_ALGORITHM_TYPE_NOX);
    }
    s->online=ok;s->ever=true;s->failures=0;s->since=am_now();set_state(i,ok?AM_WARMING:AM_FAILED);
}
static void failed(unsigned i) {
    sensor *s=&devices[i];s->failures++;
    am_lock();app.sensor_errors[i]++;am_unlock();
    if(s->failures>=3){s->online=false;set_state(i,AM_FAILED);}else set_state(i,AM_STALE);
}
bool am_sensors_calibrate(uint16_t ppm) {
    am_lock();bool valid=am_reading_current(&app.readings[AM_CO2],am_now(),15000) && am_now()-co2_started>=180000;am_unlock();
    return calibrations && ppm>=400 && ppm<=2000 && valid && xQueueSend(calibrations,&ppm,0)==pdTRUE;
}
static void task(void *unused) {
    (void)unused;
    am_config cfg,applied;am_config_get(&applied);
    vTaskDelay(pdMS_TO_TICKS(1000));
    for(unsigned i=0;i<4;i++)discover(i,&applied);
    TickType_t wake=xTaskGetTickCount();unsigned tick=0;
    for(;;) {
        am_config_get(&cfg);uint64_t now=am_now();
        if(devices[2].online && (cfg.co2_asc!=applied.co2_asc || cfg.altitude_m!=applied.altitude_m)) {
            if(!start_co2(&devices[2],&cfg))failed(2);else set_state(2,AM_WARMING);
        }
        applied=cfg;
        if(tick%30==0)for(unsigned i=0;i<4;i++)if(!devices[i].online)discover(i,&cfg);
        if(devices[0].online) {
            uint8_t cmd=0xfd,b[6];float t,rh;
            bool ok=i2c_master_transmit(devices[0].dev,&cmd,1,100)==ESP_OK;
            vTaskDelay(pdMS_TO_TICKS(10));
            ok=ok && i2c_master_receive(devices[0].dev,b,6,100)==ESP_OK && am_decode_sht4x(b,&t,&rh);
            if(ok){publish(AM_TEMP,t+cfg.temperature_offset,AM_VALID);publish(AM_RH,rh,AM_VALID);devices[0].failures=0;}else failed(0);
        }
        if(devices[1].online) {
            uint8_t b[32];uint16_t pm[3];
            if(i2c_master_receive(devices[1].dev,b,sizeof(b),100)==ESP_OK && am_decode_pmsa003i(b,sizeof(b),pm)) {
                am_state state=now-devices[1].since>=30000?AM_VALID:AM_WARMING;
                for(int i=0;i<3;i++)publish(AM_PM1+i,pm[i],state);
                devices[1].failures=0;
            }else failed(1);
        }
        if(devices[2].online && tick%5==0) {
            uint16_t ready=0,w[3];
            bool ok=command(&devices[2],0xe4b8,NULL,0,&ready,1,2);
            if(ok && (ready&0x7ff)) {
                ok=command(&devices[2],0xec05,NULL,0,w,3,2)&&w[0]>0;
                if(ok){publish(AM_CO2,w[0],AM_VALID);devices[2].failures=0;}
            }
            if(!ok)failed(2);
        }
        if(devices[3].online) {
            am_reading t,rh;am_lock();t=app.readings[AM_TEMP];rh=app.readings[AM_RH];am_unlock();
            bool compensation=am_reading_current(&t,am_now(),5000)&&am_reading_current(&rh,am_now(),5000);
            uint16_t args[2]={0x8000,0x6666},w[2];
            if(compensation){args[0]=(uint16_t)lroundf(fminf(100,fmaxf(0,rh.value))*65535/100);args[1]=(uint16_t)lroundf((fminf(130,fmaxf(-45,t.value))+45)*65535/175);}
            bool conditioning=am_now()-devices[3].since<10000;
            bool ok=command(&devices[3],conditioning?0x2612:0x2619,args,2,w,conditioning?1:2,50);
            if(ok) {
                devices[3].failures=0;
                if(conditioning){set_state(3,AM_WARMING);}
                else {
                    int32_t vi=0,ni=0;GasIndexAlgorithm_process(&voc,w[0],&vi);GasIndexAlgorithm_process(&nox,w[1],&ni);
                    /* Datasheet index settling: VOC 1.5 h, NOx 6 h. */
                    uint64_t elapsed=am_now()-devices[3].since;
                    publish(AM_VOC,vi,compensation?(elapsed>=5400000&&vi>0?AM_VALID:AM_WARMING):AM_STALE);
                    publish(AM_NOX,ni,compensation?(elapsed>=21600000&&ni>0?AM_VALID:AM_WARMING):AM_STALE);
                }
            }else failed(3);
        }
        uint16_t reference;
        if(xQueueReceive(calibrations,&reference,0)==pdTRUE) {
            set_state(2,AM_WARMING);uint16_t correction;
            bool ok=command(&devices[2],0x3f86,NULL,0,NULL,0,500) && command(&devices[2],0x362f,&reference,1,&correction,1,400) && correction!=0xffff;
            bool restart=start_co2(&devices[2],&cfg);
            am_lock();strlcpy(app.config_result,ok&&restart?"CO2 calibration complete":"CO2 calibration failed",sizeof(app.config_result));am_unlock();
            if(!restart)failed(2);
        }
        am_lock();
        for(int i=0;i<AM_METRICS;i++)if(app.readings[i].state==AM_VALID && !am_reading_current(&app.readings[i],am_now(),i==AM_CO2?15000:5000))app.readings[i].state=AM_STALE;
        am_history_sample(&app.history,am_now(),am_utc(),app.readings);
        am_unlock();tick++;vTaskDelayUntil(&wake,pdMS_TO_TICKS(1000));
    }
}
void am_sensors_start(void) {
    i2c_master_bus_config_t bc={.i2c_port=I2C_NUM_0,.sda_io_num=19,.scl_io_num=18,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=false};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bc,&bus));
    for(unsigned i=0;i<4;i++){i2c_device_config_t dc={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=devices[i].address,.scl_speed_hz=100000};ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dc,&devices[i].dev));}
    calibrations=xQueueCreate(1,sizeof(uint16_t));configASSERT(calibrations);
    configASSERT(xTaskCreate(task,"sensors",6144,NULL,5,NULL)==pdPASS);
}

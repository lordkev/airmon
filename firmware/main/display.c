#include "airmon.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_heap_caps.h"
#include "lvgl.h"

static esp_lcd_panel_handle_t panel;
static spi_device_handle_t touch;
static lv_display_t *display;
static lv_obj_t *cards[AM_METRICS],*status_label,*chart,*chart_label,*screen;
static lv_chart_series_t *series;
static const char *const titles[]={"Temperature","Humidity","PM1","PM2.5","PM10","CO2","VOC index","NOx index"};
static unsigned page=0,metric=AM_PM25,cal_step;
static uint64_t last_input;
static bool calibrating=false,wait_release=false;
static volatile bool recalibration_requested;
static float calibration_raw[4][2];
static const float targets[4][2]={{30,30},{290,30},{30,210},{290,210}};

static bool flushed(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *event,void *context) {
    (void)io;(void)event;lv_display_flush_ready(context);return false;
}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels) {
    (void)d;lv_draw_sw_rgb565_swap(pixels,lv_area_get_width(area)*lv_area_get_height(area));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels));
}
static uint16_t read_axis(uint8_t command) {
    spi_transaction_t t={.flags=SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA,.length=24};t.tx_data[0]=command;
    if(spi_device_polling_transmit(touch,&t)!=ESP_OK)return 0;
    return (((uint16_t)t.rx_data[1]<<8)|t.rx_data[2])>>3;
}
static bool raw_touch(float *x,float *y) {
    if(gpio_get_level(GPIO_NUM_36))return false;
    uint16_t xs[5],ys[5];for(int i=0;i<5;i++){xs[i]=read_axis(0xd0);ys[i]=read_axis(0x90);}
    for(int i=1;i<5;i++)for(int j=i;j>0;j--){if(xs[j]<xs[j-1]){uint16_t t=xs[j];xs[j]=xs[j-1];xs[j-1]=t;}if(ys[j]<ys[j-1]){uint16_t t=ys[j];ys[j]=ys[j-1];ys[j-1]=t;}}
    if(xs[2]<50||ys[2]<50||xs[2]>4045||ys[2]>4045)return false;
    *x=xs[2];*y=ys[2];return true;
}
static void read_touch(lv_indev_t *input,lv_indev_data_t *data) {
    (void)input;float rx,ry;bool down=raw_touch(&rx,&ry);data->state=LV_INDEV_STATE_RELEASED;
    if(down)last_input=am_now();
    if(calibrating)return;
    am_lock();bool calibrated=app.touch_calibrated;float a[6];memcpy(a,app.touch_affine,sizeof(a));am_unlock();
    if(!down||!calibrated)return;
    float x,y;am_touch_map(a,rx,ry,&x,&y);data->point.x=(int)fminf(319,fmaxf(0,x));data->point.y=(int)fminf(239,fmaxf(0,y));data->state=LV_INDEV_STATE_PRESSED;
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width,const lv_font_t *font) {
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,text);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,width);lv_obj_set_style_text_font(l,font,0);return l;
}
static void show_page(unsigned p);
static void nav(lv_event_t *e){show_page((unsigned)(uintptr_t)lv_event_get_user_data(e));}
static void open_metric(lv_event_t *e){metric=(unsigned)(uintptr_t)lv_event_get_user_data(e);show_page(1);}
static lv_obj_t *button(const char *text,int x,int y,int w,int h,lv_event_cb_t cb,void *data) {
    lv_obj_t *b=lv_button_create(screen);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,data);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,text);lv_obj_center(l);return b;
}
static void setup(lv_event_t *e){(void)e;am_network_setup();show_page(3);}
static void calibrate(lv_event_t *e){(void)e;recalibration_requested=true;}
static void brightness(lv_event_t *e) {
    am_config c;am_config_get(&c);c.brightness=lv_slider_get_value(lv_event_get_target(e));am_network_configure(&c);last_input=am_now();
}
static void show_page(unsigned p) {
    page=p;calibrating=false;lv_obj_clean(screen);memset(cards,0,sizeof(cards));chart=NULL;status_label=NULL;chart_label=NULL;
    if(p==0) {
        label(screen,"AIRMON",10,5,150,&lv_font_montserrat_18);
        button("Settings",224,2,88,32,nav,(void*)2);
        const unsigned order[]={AM_TEMP,AM_RH,AM_PM25,AM_CO2,AM_PM1,AM_PM10,AM_VOC,AM_NOX};
        for(int n=0;n<AM_METRICS;n++) {
            unsigned i=order[n];int x=8+(n%2)*156,y=40+(n/2)*43;
            lv_obj_t *b=button("",x,y,148,39,open_metric,(void*)(uintptr_t)i);
            lv_obj_set_style_pad_all(b,3,0);label(b,titles[i],1,0,143,&lv_font_montserrat_14);
            cards[i]=label(b,"--",1,17,143,&lv_font_montserrat_18);
        }
        status_label=label(screen,"Starting...",8,216,306,&lv_font_montserrat_14);
    } else if(p==1) {
        label(screen,titles[metric],8,8,210,&lv_font_montserrat_18);button("Back",242,2,70,34,nav,NULL);
        chart_label=label(screen,"",8,37,302,&lv_font_montserrat_14);
        chart=lv_chart_create(screen);lv_obj_set_pos(chart,12,70);lv_obj_set_size(chart,294,135);lv_chart_set_type(chart,LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(chart,180);series=lv_chart_add_series(chart,lv_color_hex(0x31d5b4),LV_CHART_AXIS_PRIMARY_Y);
        label(screen,"Up to 24 h / resets on reboot",12,216,300,&lv_font_montserrat_14);
    } else if(p==2) {
        label(screen,"Settings",8,6,200,&lv_font_montserrat_18);button("Back",242,2,70,34,nav,NULL);
        label(screen,"Brightness",12,45,120,&lv_font_montserrat_14);
        lv_obj_t *slider=lv_slider_create(screen);lv_obj_set_pos(slider,135,49);lv_obj_set_size(slider,165,16);lv_slider_set_range(slider,5,100);
        am_config c;am_config_get(&c);lv_slider_set_value(slider,c.brightness,LV_ANIM_OFF);lv_obj_add_event_cb(slider,brightness,LV_EVENT_RELEASED,NULL);
        button("Wi-Fi setup / credentials",10,84,300,40,setup,NULL);
        button("Calibrate touchscreen",10,134,300,40,calibrate,NULL);
        status_label=label(screen,"",12,190,298,&lv_font_montserrat_14);
    } else {
        label(screen,"Connect with your phone",8,5,300,&lv_font_montserrat_18);
        char text[340];snprintf(text,sizeof(text),"Wi-Fi: %s\nPassword: %s\nSetup: http://192.168.4.1\n\nAdministrator token:\n%.16s\n%s",app.ap_ssid,app.ap_password,app.admin,app.admin+16);
        label(screen,text,10,38,300,&lv_font_montserrat_14);
        button("Back",220,200,90,34,nav,NULL);
    }
}
static void draw_calibration(void) {
    lv_obj_clean(screen);label(screen,"Tap the cross, then release",15,100,295,&lv_font_montserrat_18);
    lv_obj_t *cross=label(screen,"+",targets[cal_step][0]-10,targets[cal_step][1]-12,24,&lv_font_montserrat_24);
    lv_obj_set_style_text_color(cross,lv_color_hex(0x31d5b4),0);
}
static void update_calibration(void) {
    float x,y;bool down=raw_touch(&x,&y);
    if(!down){wait_release=false;return;}if(wait_release)return;
    calibration_raw[cal_step][0]=x;calibration_raw[cal_step][1]=y;wait_release=true;cal_step++;
    if(cal_step<4){draw_calibration();return;}
    float affine[6];if(am_touch_calibrate(calibration_raw,targets,affine)){am_touch_save(affine);show_page(0);}
    else {cal_step=0;draw_calibration();}
}
static void refresh(void) {
    am_reading r[AM_METRICS];char ip[16];bool wifi;am_lock();memcpy(r,app.readings,sizeof(r));strlcpy(ip,app.ip,sizeof(ip));wifi=app.wifi_connected;am_unlock();
    for(int i=0;i<AM_METRICS;i++)if(cards[i]) {
        char text[50];if(am_reading_current(&r[i],am_now(),i==AM_CO2?15000:5000))snprintf(text,sizeof(text),i<2?"%.1f %s":"%.0f %s",r[i].value,i==AM_TEMP?"C":i==AM_RH?"%":i==AM_CO2?"ppm":i>=AM_VOC?"":"ug/m3");
        else strlcpy(text,r[i].state==AM_ABSENT?"Not installed":am_state_names[r[i].state],sizeof(text));
        lv_label_set_text(cards[i],text);
    }
    if(status_label){char text[96];snprintf(text,sizeof(text),wifi?"%s / v%s":"Offline / BOOT 5s: Wi-Fi setup",ip,AM_VERSION);lv_label_set_text(status_label,text);}
    if(chart) {
        int32_t values[180];for(int i=0;i<180;i++)values[i]=LV_CHART_POINT_NONE;int32_t min=INT32_MAX,max=INT32_MIN;
        am_lock();size_t n=app.history.count;size_t stride=(n+179)/180;if(!stride)stride=1;
        size_t slot=0;for(size_t i=0;i<n&&slot<180;i+=stride,slot++) {
            double sum=0;unsigned count=0;for(size_t k=i;k<n&&k<i+stride;k++){const am_history_point *p=am_history_at(&app.history,k);if(p->values[metric]!=AM_INVALID){sum+=p->values[metric];count++;}}
            if(count){values[slot]=(int32_t)lround(sum/count);if(values[slot]<min)min=values[slot];if(values[slot]>max)max=values[slot];}
        }am_unlock();
        if(min==INT32_MAX){min=0;max=am_scales[metric]*100;}if(max==min)max+=am_scales[metric];
        lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,min,max);
        for(unsigned i=0;i<180;i++)lv_chart_set_value_by_id(chart,series,i,values[i]);
        char text[100];snprintf(text,sizeof(text),"%.1f to %.1f %s / %s",(double)min/am_scales[metric],(double)max/am_scales[metric],(metric==AM_TEMP?"C":metric==AM_RH?"%":metric==AM_CO2?"ppm":metric>=AM_VOC?"index":"ug/m3"),am_state_names[r[metric].state]);lv_label_set_text(chart_label,text);lv_chart_refresh(chart);
    }
}
void am_display_recalibrate(void){recalibration_requested=true;}
static void ui_task(void *unused) {
    (void)unused;uint64_t previous=am_now(),next_refresh=0,pressed=0;bool long_press=false;last_input=previous;
    for(;;) {
        uint64_t now=am_now();lv_tick_inc(now-previous);previous=now;
        if(recalibration_requested){recalibration_requested=false;calibrating=true;cal_step=0;wait_release=true;memset(cards,0,sizeof(cards));status_label=chart=chart_label=NULL;draw_calibration();}
        if(!gpio_get_level(GPIO_NUM_0)) {
            if(!pressed){pressed=now;long_press=false;}last_input=now;
            if(now-pressed>=5000&&!long_press){long_press=true;am_network_setup();show_page(3);}
        }else if(pressed){if(!long_press&&now-pressed>40)show_page((page+1)%3);pressed=0;}
        if(calibrating)update_calibration();
        else if(now>next_refresh){refresh();next_refresh=now+2000;}
        am_config c;am_config_get(&c);unsigned brightness=c.brightness;if(c.dim_seconds&&now-last_input>(uint64_t)c.dim_seconds*1000)brightness=5;
        ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,brightness*1023/100);ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
        lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void am_display_start(void) {
    lv_init();display=lv_display_create(320,240);
    const size_t bytes=320*20*2;void *a=heap_caps_malloc(bytes,MALLOC_CAP_DMA),*b=heap_caps_malloc(bytes,MALLOC_CAP_DMA);configASSERT(a&&b);
    lv_display_set_buffers(display,a,b,bytes,LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
    spi_bus_config_t bus={.sclk_io_num=14,.mosi_io_num=13,.miso_io_num=12,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=bytes};
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io;esp_lcd_panel_io_spi_config_t ioc={.dc_gpio_num=2,.cs_gpio_num=15,.pclk_hz=40000000,.lcd_cmd_bits=8,.lcd_param_bits=8,.spi_mode=0,.trans_queue_depth=4,.on_color_trans_done=flushed,.user_ctx=display};
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&ioc,&io));
    esp_lcd_panel_dev_config_t pc={.reset_gpio_num=-1,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR,.bits_per_pixel=16};
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io,&pc,&panel));ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,true));ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,true,false));ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
    spi_bus_config_t tb={.sclk_io_num=25,.mosi_io_num=32,.miso_io_num=39,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=8};
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&tb,SPI_DMA_DISABLED));spi_device_interface_config_t td={.clock_speed_hz=1000000,.mode=0,.spics_io_num=33,.queue_size=1};ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST,&td,&touch));
    gpio_set_direction(GPIO_NUM_36,GPIO_MODE_INPUT);gpio_set_direction(GPIO_NUM_0,GPIO_MODE_INPUT);gpio_set_pull_mode(GPIO_NUM_0,GPIO_PULLUP_ONLY);
    ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.timer_num=LEDC_TIMER_0,.duty_resolution=LEDC_TIMER_10_BIT,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t channel={.gpio_num=21,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=307};ESP_ERROR_CHECK(ledc_channel_config(&channel));
    lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
    screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0x101e2a),0);lv_obj_set_style_text_color(screen,lv_color_hex(0xeaf1f6),0);lv_obj_set_style_pad_all(screen,0,0);lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    show_page(0);if(!app.touch_calibrated)recalibration_requested=true;
    configASSERT(xTaskCreatePinnedToCore(ui_task,"display",8192,NULL,4,NULL,1)==pdPASS);
}

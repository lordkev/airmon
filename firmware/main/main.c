#include "airmon.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

am_app app;
uint64_t am_now(void) {return (uint64_t)esp_timer_get_time()/1000;}
uint32_t am_utc(void) {time_t t=time(NULL);return t>1700000000?(uint32_t)t:0;}
void app_main(void) {
    app.mutex=xSemaphoreCreateMutex();configASSERT(app.mutex);
    /* Board Q5 supplies UART pin 1. GPIO22 also lights the red LED. */
    gpio_set_level(GPIO_NUM_22,0);gpio_set_direction(GPIO_NUM_22,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4,1);gpio_set_direction(GPIO_NUM_4,GPIO_MODE_OUTPUT); /* audio off */
    gpio_set_level(GPIO_NUM_5,1);gpio_set_direction(GPIO_NUM_5,GPIO_MODE_OUTPUT); /* deselect SD */
    gpio_set_level(GPIO_NUM_16,1);gpio_set_direction(GPIO_NUM_16,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_17,1);gpio_set_direction(GPIO_NUM_17,GPIO_MODE_OUTPUT);
    esp_err_t err=nvs_flash_init();
    /* Never erase credentials silently when storage cannot be opened. */
    ESP_ERROR_CHECK(err);
    am_config_init();
    am_network_start();
    am_sensors_start();
    am_web_start();
    am_display_start();
    vTaskDelay(pdMS_TO_TICKS(10000));
    /* Optional sensors and an unavailable router do not invalidate firmware. */
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY)
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    ESP_LOGI("airmon","Startup complete; firmware %s",AM_VERSION);
}

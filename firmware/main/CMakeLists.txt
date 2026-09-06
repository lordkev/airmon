idf_component_register(SRCS "main.c" "config.c" "sensors.c" "network.c" "web.c" "display.c"
    INCLUDE_DIRS "."
    REQUIRES airmon_core gas_index nvs_flash esp_wifi esp_netif esp_event esp_http_server
    app_update esp_timer esp_driver_gpio esp_driver_i2c esp_driver_spi esp_driver_ledc
    esp_lcd json mqtt mbedtls esp_system esp_app_format lwip
    EMBED_FILES "web/index.html.gz")

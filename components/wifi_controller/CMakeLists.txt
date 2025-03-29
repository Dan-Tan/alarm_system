idf_component_register(SRCS "wifi_controller.c" "connect.c"
                       INCLUDE_DIRS .
                       REQUIRES esp_wifi nvs_flash esp_http_server)

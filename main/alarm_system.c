#include <stdio.h>

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"

#define BLINK_LED 27

void app_main(void)
{
    char *taskName = pcTaskGetName(NULL);
    ESP_LOGI(taskName, "Hello World!");

    gpio_reset_pin(BLINK_LED);
    gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(BLINK_LED, 1);
        ESP_LOGI(taskName, "ON!");
        vTaskDelay(100);
        gpio_set_level(BLINK_LED, 0);
        ESP_LOGI(taskName, "OFF!");
        vTaskDelay(100);
    }
}

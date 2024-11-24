#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>

// Logging/Error includes
#include "esp_err.h"
#include "esp_log.h"

// SD card includes
#include "freertos/portmacro.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
#include "esp_sleep.h"

#include "driver/gpio.h"

#include "ulp_controller.h"
#include "audio.h"
#include "storage.h"

#define GPIO_INPUT_PIN_SEL   (1ULL << GPIO_RTC_SWITCH)

const char* MAIN_TAG = "MAIN";

bool check_mp3_suffix(char* filename) {
    int counter = 0;
    while (*filename != '\0') {
        if (counter == 128) {
            return false;
        }
        filename++;
        counter++;
    }
    return *(filename - 4) == '.' &&
        *(filename - 3) == 'm' &&
        *(filename - 2) == 'p' &&
        *(filename - 1) == '3';
}

void check_button_input(void* unused) {
    printf("Entering check button\n");
    uint32_t current_lvl = 0;
    uint32_t prev_lvl = 0;
    while (1) {
        current_lvl = gpio_get_level(GPIO_RTC_SWITCH);
        if ((prev_lvl == 1) && (current_lvl == 0)) {
            printf("Entering deep sleep\n");
            esp_sleep_enable_ext0_wakeup(GPIO_RTC_SWITCH, 1);
            esp_deep_sleep_start();
        }
        prev_lvl = current_lvl;
        vTaskDelay(10);
    }
}

void app_main(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type     = GPIO_INTR_DISABLE;
    io_conf.mode          = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask  = GPIO_INPUT_PIN_SEL;
    io_conf.pull_down_en  = 0;
    io_conf.pull_up_en    = 0;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, initializing ULP\n");
        init_ulp();
    } 

    gpio_config(&io_conf);
    
    esp_err_t ret;
    ret = set_up_storage();
    bool has_sd_card = true;
    if (ret != ESP_OK) {
        ESP_LOGW(MAIN_TAG, "Failed to mount storage device. Continuing without storage...");
        has_sd_card = false;
    }

    init_i2s_interface();
    int err = 0;

    TaskHandle_t audio_handle  = NULL; 
    if (has_sd_card) {
        FILE *config_file = fopen(MOUNT_POINT CONFIG_FILE, "r");
        if (config_file == NULL) {
            ESP_LOGE(MAIN_TAG, "Failed to config file for reading");
        }
        char audio_filename[32] = MOUNT_POINT"/";
        fgets(audio_filename + strlen(audio_filename), 32, config_file);
        fclose(config_file);
        ESP_LOGI(MAIN_TAG, "Config File Read.");
        ESP_LOG_BUFFER_CHAR(MAIN_TAG, audio_filename, 32);

        if (!check_mp3_suffix(audio_filename)) {
            ESP_LOGE(MAIN_TAG, "Audio file defined in config missing mp3 suffix. Exiting");
        } else {
            ESP_LOGI(MAIN_TAG, "MP3 file found");
        }
        err = xTaskCreate(
                play_mp3,
                "Play music",
                20000,
                &audio_filename,
                32,
                &audio_handle
                );
    }
    else {
        char _unused[2];
        err = xTaskCreate(
                sine_wave,
                "Play music",
                20000,
                &_unused,
                32,
                &audio_handle
                );

    }

    printf("Setting up tasks\n");

    TaskHandle_t button_handle = NULL; 
    err = xTaskCreate(
            check_button_input,
            "Button checker",
            2048,
            &err,
            32,
            &button_handle
            );
    printf("Err %d\n", err);
    while (1) {
        vTaskDelay(100);
    }
}

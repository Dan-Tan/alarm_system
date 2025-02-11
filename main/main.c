#define configUSE_TASK_NOTIFICATIONS 1

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>

// Logging/Error includes
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

// SD card includes
#include "freertos/portmacro.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "hal/adc_types.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "voltage_types.h"
#include "voltage.h"

static const struct voltage_read_config_t voltage_conf = {
    .channel = ADC_CHANNEL_6,
    .width = ADC_WIDTH_BIT_12,
    .atten = ADC_ATTEN_DB_12,
    .unit = ADC_UNIT_1,
    .div_coef = {.numerator = 2, .denominator = 1},
    .default_vref = 3900,
    .n_samples = 64
};

#include "driver/gpio.h"

#include "ulp_controller.h"
#include "audio.h"
#include "storage.h"

#define GPIO_AUDIO_CONTROL   39

#define GPIO_INPUT_PIN_SEL   ((1ULL << GPIO_RTC_SWITCH) | (1ULL << GPIO_AUDIO_CONTROL))

#define GPIO_PERIPHERAL_POWER  18
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_PERIPHERAL_POWER)

const char* MAIN_TAG = "MAIN";

static bool has_sd_card = true;
static TaskHandle_t *audio_handle = NULL;
static char audio_filename[32] = MOUNT_POINT"/";

void monitor_audio_toggle(void* unused) {
    uint32_t current_lvl = 0;
    uint32_t prev_lvl = 0;
    while (1) {
        current_lvl = gpio_get_level(GPIO_AUDIO_CONTROL);
        if ((prev_lvl == 1) && (current_lvl == 0)) {
            if (audio_handle) {
                xTaskNotify(*audio_handle, 0, eNoAction);
            } else {
                audio_handle = malloc(sizeof(TaskHandle_t));
                xTaskCreate(
                    has_sd_card ? play_mp3 : sine_wave,
                    "Play music",
                    20000,
                    audio_filename,
                    32,
                    audio_handle
                    );
            }
        }
        prev_lvl = current_lvl;
        vTaskDelay(10);
    }
    free(audio_handle);
    audio_handle = NULL;
}

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
    uint32_t current_lvl = 0;
    uint32_t prev_lvl = 0;
    while (1) {
        current_lvl = gpio_get_level(GPIO_RTC_SWITCH);
        if ((prev_lvl == 1) && (current_lvl == 0)) {
            esp_sleep_enable_ext0_wakeup(GPIO_RTC_SWITCH, 1);
            shut_down_storage();
            esp_deep_sleep_start();
        }
        prev_lvl = current_lvl;
        vTaskDelay(10);
    }
}

void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Configuring power button.");
    gpio_config_t power_io_conf = {};
    power_io_conf.intr_type     = GPIO_INTR_DISABLE;
    power_io_conf.mode          = GPIO_MODE_INPUT;
    power_io_conf.pin_bit_mask  = GPIO_INPUT_PIN_SEL;
    power_io_conf.pull_down_en  = 0;
    power_io_conf.pull_up_en    = 0;

    gpio_config(&power_io_conf);

    ESP_LOGI(MAIN_TAG, "Configuring peripheral power switch.");
    gpio_config_t peripheral_io_conf = {};
    peripheral_io_conf.intr_type     = GPIO_INTR_DISABLE;
    peripheral_io_conf.mode          = GPIO_MODE_OUTPUT;
    peripheral_io_conf.pin_bit_mask  = GPIO_OUTPUT_PIN_SEL;
    peripheral_io_conf.pull_down_en  = 0;
    peripheral_io_conf.pull_up_en    = 0;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        ESP_LOGI(MAIN_TAG, "Not ULP wakeup, initializing ULP");
//        init_ulp();
    } 

    gpio_config(&power_io_conf);
    gpio_config(&peripheral_io_conf);
    gpio_set_level(GPIO_PERIPHERAL_POWER, 1);

    esp_err_t ret;

    ret = adc_config(&voltage_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "Unable to configure battery voltage readings. Invalid configuration arguements.");
    }
    uint32_t voltage = 0;
    read_voltage(&voltage_conf, &voltage);
    printf("voltage: %d\n", voltage);

    ret = set_up_storage();
    if (ret != ESP_OK) {
        ESP_LOGW(MAIN_TAG, "Failed to mount storage device. Continuing without storage...");
        has_sd_card = false;
    }

    init_i2s_interface();
    int err = 0;

    if (has_sd_card) {
        FILE *config_file = fopen(MOUNT_POINT CONFIG_FILE, "r");
        if (config_file == NULL) {
            ESP_LOGE(MAIN_TAG, "Failed to config file for reading");
        }
        fgets(audio_filename + strlen(audio_filename), 32, config_file);
        fclose(config_file);
        ESP_LOGI(MAIN_TAG, "Config File Read.");
        ESP_LOG_BUFFER_CHAR(MAIN_TAG, audio_filename, 32);
    }
    printf("Setting up tasks\n");

    TaskHandle_t button_handle = NULL; 
    err = xTaskCreate(
            check_button_input,
            "Button checker",
            2048,
            &err,
            24,
            &button_handle
            );
    TaskHandle_t audio_toggle = NULL; 
    err = xTaskCreate(
            monitor_audio_toggle,
            "Audio Toggle checker",
            2048,
            &err,
            24,
            &audio_toggle
            );
    while (1) {
        vTaskDelay(1000 / portTICK_RATE_MS);

    }
}

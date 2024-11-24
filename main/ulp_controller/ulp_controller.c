#include <stdio.h>
#include <inttypes.h>

#include "freertos/portmacro.h"
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc_periph.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "esp32/ulp.h"
#include "ulp_main.h"

#include "ulp_controller.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

void init_ulp() {
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
            (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    /* GPIO used for pulse counting. */
    gpio_num_t gpio_num = GPIO_RTC_SWITCH;
    int rtcio_num = rtc_io_number_get(GPIO_RTC_SWITCH);

    ulp_io_switch_number = rtcio_num; /* map from GPIO# to RTC_IO# */
    ulp_io_switch_state = 0;

    printf("ULP Wake Up GPIO: %d, RTC_IO: %d\n", gpio_num, rtcio_num);

    /* Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown */
    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(gpio_num);
    rtc_gpio_pullup_dis(gpio_num);
    rtc_gpio_hold_en(gpio_num);
    rtc_gpio_isolate(gpio_num);

    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_15);

    esp_deep_sleep_disable_rom_logging(); // suppress boot messages

    ulp_set_wakeup_period(0, 20000);

    /* Start the program */
    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_ERROR_CHECK(err);
}

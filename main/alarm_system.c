#include <stdio.h>

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

// Logging/Error includes
#include "esp_err.h"
#include "esp_log.h"

// File system includes
#include "esp_vfs_fat.h"

// SPI interface includes
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

// SD card includes
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"

static const char *TAG = "SD Card";

#define MOUNT_POINT "/sdcard"

#define CONFIG_FILE "/config.txt"

#define PIN_NUM_MISO 27 // Master In Slave Out
#define PIN_NUM_MOSI 15 // Master Out Slave In
#define PIN_NUM_CLK  14 // Clock
#define PIN_NUM_CS   13 // Child Select

// Don't ask me why
#define SPI_DMA_CHAN host.slot

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

void app_main(void)
{
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    ESP_LOGI(TAG, "Bland or S2");
#elif CONFIG_IDF_TARGET_ESP32C3
    ESP_LOGI(TAG, "C3");
#endif
    esp_err_t ret;

    // SD Card File System Mount options
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // Don't format card if failed
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG, "Opening file");
    FILE *config_file = fopen(MOUNT_POINT CONFIG_FILE, "r");
    if (config_file == NULL) {
        ESP_LOGE(TAG, "Failed to config file for reading");
        return;
    }
    char audio_filename[128] = MOUNT_POINT"/";
    fgets(audio_filename + strlen(audio_filename), 128, config_file);
    fclose(config_file);
    ESP_LOGI(TAG, "Config File Read.");

    if (!check_mp3_suffix(audio_filename)) {
        ESP_LOGE(TAG, "Audio file defined in config missing mp3 suffix. Exiting");
        return;
    } else {
        ESP_LOGI(TAG, "MP3 file found");
    }

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(audio_filename, &st) == 0) {
        ESP_LOGE(TAG, "Audio file defined in config not found. Exiting");
        return;
    } else {
        ESP_LOGI(TAG, "Audio file found");
    }

    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
#ifdef USE_SPI_MODE
    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
#endif
}

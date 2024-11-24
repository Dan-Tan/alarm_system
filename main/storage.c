// Logging/Error includes
#include "esp_err.h"
#include "esp_log.h"

// File system includes
#include "esp_vfs_fat.h"

// SPI interface includes
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

// SD Card command libraries
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "storage.h"

const char *SD_TAG = "SD Card";

static sdmmc_card_t *card = NULL;
static sdmmc_host_t *host = NULL;

esp_err_t set_up_storage() {
    esp_err_t ret;
    
    // File system configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // Don't format card if failed
        .max_files = 5, // max number of open files
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(SD_TAG, "Initializing SD card");
    
    host = malloc(sizeof(sdmmc_host_t));
    *host = (sdmmc_host_t) SDSPI_HOST_DEFAULT();

    // SPI Interface configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host->slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(SD_TAG, "Failed to initialize bus.");
        return ret;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host->slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). "
                    "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(SD_TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card); return ESP_OK;
}

void shut_down_storage() {
    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    ESP_LOGI(SD_TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host->slot);
    free(host);
    host = NULL;
}

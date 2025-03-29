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

// std
#include <stdio.h>
#include <string.h>

#include "storage.h"

#define MAX_CONFIG_LINE_LENGTH 256

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

size_t _max(const size_t x, const size_t y) {
    if (x > y) {
        return x;
    } else {
        return y;
    }
}

size_t _min(const size_t x, const size_t y) {
    if (x > y) {
        return y;
    } else {
        return x;
    }
}

bool _sub_str_equal(const char* str1, const size_t n1, const char* str2, const size_t n2) {
    bool eq = true;
    for (int i = 0; i < _min(n1, n2); i++) {
        eq = eq && str1[i] == str2[i];
    }
    return eq;
}

size_t extract_value(const char* source, const size_t src_size, char** destination) {
    bool found_eq_sign = false;
    bool found_quot = false;
    
    int val_start = -1;
    int val_end   = -1;
    for (int i = 0; i < src_size; i++) {
        if (!found_eq_sign && source[i] == '=') {
            found_eq_sign = true;
        }
        if (!found_quot && found_eq_sign && source[i] == '"') {
            found_quot = true;
            val_start = i + 1;
        }
        if (found_quot && source[i] == '"') {
            val_end = i;
        }
    }
    printf("line buffer: %s\n", source);
    printf("val_start: %d, val_end: %d\n", val_start, val_end);

    if (val_start == -1 || val_end == -1) {
        // did not found starting or ending quotation marks
        *destination = NULL;
        return 0;
    }
    
    if (val_start == src_size - 1) {
        // start size is next to end of line
        *destination = NULL;
        return 0;
    }
    const size_t dest_buffer_size = val_end - val_start + 1;
    *destination = (char*) malloc(dest_buffer_size * sizeof(char));

    if (!*destination) {
        // Failed to allocate
        return 0;
    }
    
    printf("destination_ptr; %p, size: destination_buffer_size: %d\n", *destination, (int)dest_buffer_size);
    (*destination)[dest_buffer_size - 1] = '\0';
    
    memcpy(*destination, source + val_start, dest_buffer_size - 1);

    return dest_buffer_size;
}

/**
 * @param char* line_buffer a preallocaed character buffer.
 * @param const size_t buffer_size, size of line_buffer
 * @param FILE* file, file pointer to read from.
 */
size_t read_line(char* line_buffer, const size_t buffer_size, FILE* file) {
    memset(line_buffer, 0, buffer_size);
    line_buffer = fgets(line_buffer, buffer_size, file);
    char* new_line_c = strchr(line_buffer, '\n');
    if (!new_line_c) {
        return 0;
    }
    size_t line_len = new_line_c - line_buffer;
    line_buffer[line_len] = '\0';
    return line_len;
}

esp_err_t extract_ap_credentials(FILE *config_file, char **ssid_buffer, char **pass_buffer) {
    static const char ssid_key[] = "ssid";
    static const char pass_key[] = "password";
    
    bool extracted_pass = false;
    bool extracted_ssid = false;
    
    *ssid_buffer = NULL;
    *pass_buffer = NULL;

    size_t line_len = 0;
    char *line_buffer = (char*) malloc(MAX_CONFIG_LINE_LENGTH * sizeof(char));

    line_len = read_line(line_buffer, MAX_CONFIG_LINE_LENGTH, config_file);
    if (line_len == 0) {
        ESP_LOGE(SD_TAG, "Found end of config file before extracting AP credentials.");
    }

    if (_sub_str_equal(ssid_key, 4, line_buffer, line_len) && line_len > 0) {
        extract_value(line_buffer, line_len, ssid_buffer);    
        ESP_LOG_BUFFER_CHAR(SD_TAG, ssid_buffer, strlen(*ssid_buffer));
        extracted_ssid = true;
    }
    else {
        ESP_LOGE(SD_TAG, "Expected ssid key after AP credential config header.");
    }

    line_len = read_line(line_buffer, MAX_CONFIG_LINE_LENGTH, config_file);
    if (line_len == 0) {
        ESP_LOGE(SD_TAG, "Found end of config file before extracting AP credentials.");
    }

    if (_sub_str_equal(pass_key, 8, line_buffer, line_len) && line_len > 0) {
        extract_value(line_buffer, line_len, pass_buffer);    
        ESP_LOG_BUFFER_CHAR(SD_TAG, pass_buffer, strlen(*pass_buffer));
        extracted_pass = true;
    }
    else {
        ESP_LOGE(SD_TAG, "Expected password key after AP ssid key.");
    }

    free(line_buffer);

    if (!(extracted_ssid && extracted_pass)) {
        free(*ssid_buffer);
        free(*pass_buffer);
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

bool is_wifi_section(const char* buffer, const size_t n) {
    static const char wifi_header[] = "[WiFi AP Credentials]";
    static const size_t header_len = 21;
    const size_t buffer_len = (buffer[n - 1] == '\n') ? n - 1 : n;
    return _sub_str_equal(wifi_header, header_len, buffer, buffer_len);
}

esp_err_t get_ap_credentials(const char* config_filepath, char **ssid, char **password) {
    FILE *config_file = fopen(config_filepath, "r");

    size_t line_len = 0;
    char *line_buffer = (char*) malloc(MAX_CONFIG_LINE_LENGTH * sizeof(char));
    
    while ((line_len = read_line(line_buffer, MAX_CONFIG_LINE_LENGTH, config_file)) != 0) {
        ESP_LOGI(SD_TAG, "Readling line from configuration file.");
        ESP_LOG_BUFFER_CHAR(SD_TAG, line_buffer, 128);
        if (!is_wifi_section(line_buffer, line_len)) {
            continue;
        }
        ESP_LOGI(SD_TAG, "Found AP credentials sections");
        
        esp_err_t err = extract_ap_credentials(config_file, ssid, password);
        if (!(err == ESP_OK)) {
            ESP_LOGE(SD_TAG, "Failed to extract AP credentials.");
            free(line_buffer);
            return err;
        } else {
            ESP_LOGE(SD_TAG, "SSID");
            ESP_LOG_BUFFER_CHAR(SD_TAG, *ssid, MAX_CONFIG_LINE_LENGTH);
            ESP_LOGE(SD_TAG, "Password");
            ESP_LOG_BUFFER_CHAR(SD_TAG, *password, MAX_CONFIG_LINE_LENGTH);
            break;
        }
    }
    free(line_buffer);

    return ESP_OK;
}

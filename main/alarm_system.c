#include <stdio.h>

#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/projdefs.h"
#include "hal/i2s_types.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "driver/sdmmc_host.h"

// I2S includes for audio
#include "driver/i2s.h"

// MP3 includes
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO
#define MINIMP3_ENABLE_RING 0
#include "minimp3.h"

#define XSTR(x) STR(x)
#define STR(x) #x

static const char *SD_TAG = "SD Card";

#define MOUNT_POINT            "/sd"

#define CONFIG_FILE            "/config.txt"

// SPI pins for SD card
#define PIN_NUM_MISO            27    // Master In Slave Out
#define PIN_NUM_MOSI            15    // Master Out Slave In
#define PIN_NUM_CLK             14    // Clock
#define PIN_NUM_CS              13    // Child Select

// I2S pins for amp
#define I2S_LRC                 25    // Left Right Clock (A.K.A. WS)
#define I2S_BCLK                32    // Bit Clock
#define I2S_DOUT                33    // Data Out ESP32 (Data In Amp)
#define I2S_UNUSED              (-1)  // Not used

#define AUDIO_GAIN_PIN #define I2S_PORT_NUM            (0)
#define SAMPLE_RATE             44100
#define PI                      (3.14159265)
#define WAVE_FREQ_HZ            (400)
#define BIT_PER_SAMPLE          16
#define SAMPLE_PER_CYCLE        (SAMPLE_RATE/WAVE_FREQ_HZ)

#define AUDIO_BUFFER_SIZE       16000

static const char *AUDIO_TAG = "Audio";

#define SPI_DMA_CHAN            host.slot

void init_i2s_interface() {
    esp_err_t ret;
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,  // Control and Transmit
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };
    
    ret = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(AUDIO_TAG, "Successfully installed i2s driver.");
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(AUDIO_TAG, "Invalid Argument in i2s driver install");
        return;
    } else if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(AUDIO_TAG, "Out of Memory: Unable to install i2s driver.");
        return;
    } else {
        ESP_LOGE(AUDIO_TAG, "Unknown error return in i2s driver install.");
        return;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    ESP_LOGI(AUDIO_TAG, "Setting Pin Configuration.");
    ret = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (ret == ESP_OK) {
        ESP_LOGI(AUDIO_TAG, "Successfully set i2s pin coniguration.");
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(AUDIO_TAG, "Invalid Argument in setting i2s pin configuration.");
        return;
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(AUDIO_TAG, "IO Error in setting i2s pin configuration.");
        return;
    } else {
        ESP_LOGE(AUDIO_TAG, "Unknown error return in setting i2s pin configuration.");
        return;
    }
};

void play_music(void *filepath_v) {
    //struct stat st;
    //if (stat(filepath, &st) == 0) {
    //    ESP_LOGE(SD_TAG, "Audio file defined in config not found. Exiting");
    //    return;
    //} else {
    //    ESP_LOGI(SD_TAG, "Audio file found");
    //}
    const char* filepath = (char*) filepath_v;
    mp3dec_t *mp3d = malloc(sizeof(mp3dec_t));
    mp3dec_init(mp3d);
    
    FILE *audio_file = fopen(filepath, "r");
    unsigned char *input_buffer = malloc(AUDIO_BUFFER_SIZE * sizeof(unsigned char));
    short *output_buffer = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(short));
    
    printf("Input Buffer Size: %d\n", AUDIO_BUFFER_SIZE);
    printf("Output Buffer Size: %d\n", MINIMP3_MAX_SAMPLES_PER_FRAME);
    if (!audio_file) {
        ESP_LOGE(AUDIO_TAG, "Failed to open audio file.");
    }

    if (!input_buffer) {
        ESP_LOGE(AUDIO_TAG, "Input Buffer failed to allocate.");
        return;
    }

    if (!output_buffer) {
        ESP_LOGE(AUDIO_TAG, "Output Buffer failed to allocate.");
        return;
    }
    
    size_t i2s_bytes_written = 0;
    size_t bytes_to_read = AUDIO_BUFFER_SIZE;
    int input_buffer_size = 0;
    mp3dec_frame_info_t frame_info; 

    while (1) {
        unsigned long bytes_read = fread(input_buffer + input_buffer_size, sizeof(char), bytes_to_read, audio_file);
        input_buffer_size += bytes_read;

        if (input_buffer_size == 0) {
            break;
        }

        int samples_decoded = mp3dec_decode_frame(
            mp3d, 
            input_buffer, 
            input_buffer_size, 
            output_buffer, 
            &frame_info
        );

        input_buffer_size -= frame_info.frame_bytes;
        memmove(input_buffer, input_buffer + frame_info.frame_bytes, input_buffer_size);
    
        // Refill input buffer in next loop
        bytes_to_read = frame_info.frame_bytes;

        if (samples_decoded == 0) {
            continue;
        }

        if (frame_info.channels == 1) {
          for (int i = samples_decoded - 1; i >= 0; i--) {
            output_buffer[i * 2] = output_buffer[i];
            output_buffer[i * 2 - 1] = output_buffer[i];
          }
        }

        i2s_set_clk(I2S_PORT_NUM, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, 2);

        i2s_write(I2S_PORT_NUM, output_buffer, samples_decoded * sizeof(short), &i2s_bytes_written, 100);
        if (samples_decoded * sizeof(short) > i2s_bytes_written) {
            printf("Decoded: %d, Written: %u\n", (int)(samples_decoded * sizeof(short)), i2s_bytes_written);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    };
    // Clean up
    free(input_buffer);
    fclose(audio_file);

    free(output_buffer);
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

void app_main(void)
{
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    ESP_LOGI(SD_TAG, "Bland or S2");
#elif CONFIG_IDF_TARGET_ESP32C3
    ESP_LOGI(SD_TAG, "C3");
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
    ESP_LOGI(SD_TAG, "Initializing SD card");

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
        ESP_LOGE(SD_TAG, "Failed to initialize bus.");
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
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(SD_TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(SD_TAG, "Opening file");
    FILE *config_file = fopen(MOUNT_POINT CONFIG_FILE, "r");
    if (config_file == NULL) {
        ESP_LOGE(SD_TAG, "Failed to config file for reading");
        return;
    }
    char audio_filename[32] = MOUNT_POINT"/";
    fgets(audio_filename + strlen(audio_filename), 32, config_file);
    fclose(config_file);
    ESP_LOGI(SD_TAG, "Config File Read.");
    ESP_LOG_BUFFER_CHAR(SD_TAG, audio_filename, 32);

    if (!check_mp3_suffix(audio_filename)) {
        ESP_LOGE(SD_TAG, "Audio file defined in config missing mp3 suffix. Exiting");
        return;
    } else {
        ESP_LOGI(SD_TAG, "MP3 file found");
    }

    // Check if destination file exists before renaming
    //struct stat st;
    //if (stat(audio_filename, &st) == 0) {
    //    ESP_LOGE(SD_TAG, "Audio file defined in config not found. Exiting");
    //    return;
    //} else {
    //    ESP_LOGI(SD_TAG, "Audio file found");
    //}
    printf("Before interface init\n");
    init_i2s_interface();
    printf("Before play music\n");
    FILE *audio_file = fopen(audio_filename, "r");
    if (!audio_file) {
        ESP_LOGE(SD_TAG, "Failed to open audio in main.");
    }
    fclose(audio_file);
    TaskHandle_t xHandle = NULL; 
    int err = xTaskCreate(
        play_music,
        "Play music",
        20000,
        &audio_filename,
        32,
        &xHandle
    );
    printf("Err %d\n", err);
    while (1) {
        vTaskDelay(10000);
    }
    // All done, unmount partition and disable SDMMC or SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(SD_TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}

#ifndef _STORAGE_H
#define _STORAGE_H

#define MOUNT_POINT            "/sd"

#define CONFIG_FILE            "/config.txt"
// SPI pins for SD card
#define PIN_NUM_MISO            19    // Master In Slave Out
#define PIN_NUM_MOSI            15    // Master Out Slave In
#define PIN_NUM_CLK             14    // Clock
#define PIN_NUM_CS              13    // Child Select

#define SPI_DMA_CHAN            host->slot

esp_err_t set_up_storage();
void shut_down_storage();

#endif

#include "driver/spi_master.h"
#include "esp_err.h"

#define HOST HSPI_HOST
#define DMA_CHAN 2

#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 13

esp_err_t w25q128_init(void);

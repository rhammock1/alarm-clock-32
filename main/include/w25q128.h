#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define HOST HSPI_HOST
#define DMA_CHAN 2

#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 15

#define W25Q128_CMD_WRITE_ENABLE 0x06
#define W25Q128_CMD_PROGRAM_PAGE 0x02
#define W25Q128_CMD_READ_DATA 0x03
#define W25Q128_CMD_CHIP_ERASE 0x60
#define W25Q128_CMD_SECTOR_ERASE 0x20
#define W25Q128_CMD_READ_S1 0x05
#define W25Q128_CMD_READ_S2 0x35
#define W25Q128_CMD_READ_S3 0x15

#define W25Q128_WRITE_IN_PROGRESS_BIT 0x01
#define W25Q128_WRITE_ENABLE_LATCH_BIT 0x02

esp_err_t w25q128_init(spi_device_handle_t handle);
esp_err_t w25q128_write_enable(spi_device_handle_t handle, spi_transaction_t t);
int w25q128_is_write_enabled(spi_device_handle_t handle, spi_transaction_t t);
int w25q128_write_is_in_progress(spi_device_handle_t handle, spi_transaction_t t);
esp_err_t w25q128_read_data(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr, void *data, size_t len);
esp_err_t w25q128_write_data(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr, const void *data, size_t len);
esp_err_t w25q128_sector_erase(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr);
esp_err_t w25q128_chip_erase(spi_device_handle_t handle);
esp_err_t w25q128_request_erase_chip();
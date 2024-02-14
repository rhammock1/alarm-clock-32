#include <string.h>
#include "esp_log.h"
#include "w25q128.h"

#define CMD_READ_ID 0x9F

esp_err_t read_flash_capacity(spi_device_handle_t handle)
{
  esp_err_t ret;

  uint8_t cmd = CMD_READ_ID;
  uint8_t id[3];

  spi_transaction_t t;
  memset(&t, 0, sizeof(t)); // Zero out the transaction
  t.length = 8;             // Command is 8 bits
  t.tx_buffer = &cmd;       // The data is the command
  t.rxlength = 8 * 3;       // Receive 24 bits back
  t.rx_buffer = id;         // Output buffer

  ret = spi_device_transmit(handle, &t); // Transmit!
  if(ret != ESP_OK) {
    ESP_LOGE("SPI Flash", "Error reading flash capacity: %d", ret);
    return ret;
  }
  // assert(ret == ESP_OK);                 // Should have had no issues.

  // The manufacturer ID, memory type, and capacity are now in id[0], id[1], and id[2]
  ESP_LOGI("SPI Flash", "Manufacturer ID: 0x%X, Memory Type: 0x%X, Capacity: 0x%X", id[0], id[1], id[2]);
  return ESP_OK;

  // You can then look up the capacity based on the capacity ID (id[2])
  // For the W25Q128, the capacity ID should be 0x18, which corresponds to a capacity of 128M-bit (16M-byte)
}

esp_err_t w25q128_init(void) {
  // Initialize SPI
  esp_err_t ret;

  // Initialize the SPI bus
  spi_bus_config_t buscfg = {
    .miso_io_num = PIN_NUM_MISO,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4096};
  ret = spi_bus_initialize(HOST, &buscfg, DMA_CHAN);
  assert(ret == ESP_OK);

  // Initialize the SPI device
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
    .mode = 0,                          // SPI mode 0
    .spics_io_num = PIN_NUM_CS,         // CS pin
    .queue_size = 7,                    // We want to be able to queue 7 transactions at a time
  };
  spi_device_handle_t handle;
  ret = spi_bus_add_device(HOST, &devcfg, &handle);
  assert(ret == ESP_OK);

  // Use the SPI device
  // You can use the handle to send and receive data with spi_device_transmit
  ret = read_flash_capacity(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }
  return ESP_OK;
}
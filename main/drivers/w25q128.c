#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "w25q128.h"

static const char *TAG = "W25Q128";

#define CMD_READ_ID 0x4B

esp_err_t read_device_id(spi_device_handle_t handle)
{
  esp_err_t ret;

  uint8_t cmd = CMD_READ_ID;
  uint8_t dummy_byte = 0x00;
  uint8_t id[8];

  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = 8;   // Command is 8 bits
  t.tx_buffer = &cmd;             // The data is the command
  t.flags = SPI_TRANS_CS_KEEP_ACTIVE; // Keep CS active after data transfer
  ret = spi_device_transmit(handle, &t);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read ID command: %d", ret);
    return ret;
  }

  // Dummy cycles
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = (8 * 4);             // 4 dummy bytes
  t.tx_buffer = &dummy_byte;
  t.flags = SPI_TRANS_CS_KEEP_ACTIVE; // Keep CS active after data transfer
  ret = spi_device_transmit(handle, &t);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending dummy cycles: %d", ret);
    return ret;
  }

  // Read the ID
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = 8 * 8;               // Receive 64 bits back
  t.rx_buffer = &id;              // Receive 64 bits back
  ret = spi_device_transmit(handle, &t);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading flash capacity: %d", ret);
    return ret;
  }

  spi_device_release_bus(handle);

  // Print the ID
  ESP_LOGI(TAG, "ID: %02X %02X %02X %02X %02X %02X %02X %02X", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
  return ESP_OK;
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
    .quadhd_io_num = -1};
  ret = spi_bus_initialize(HOST, &buscfg, DMA_CHAN);
  assert(ret == ESP_OK);

  // Initialize the SPI device
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
    .mode = 0,                          // SPI mode 0
    .spics_io_num = PIN_NUM_CS,         // CS pin
    .queue_size = 7,                    // We want to be able to queue 7 transactions at a time
    // .dummy_bits = 4,                    // 4 clock cycles between command and receiving data
  };
  spi_device_handle_t handle;
  ret = spi_bus_add_device(HOST, &devcfg, &handle);
  assert(ret == ESP_OK);

  // Use the SPI device
  // You can use the handle to send and receive data with spi_device_transmit
  ret = read_device_id(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }
  return ESP_OK;
}
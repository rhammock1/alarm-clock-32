#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "w25q128.h"

static const char *TAG = "W25Q128";

esp_err_t spi_cmd(spi_device_handle_t handle, spi_transaction_t t, uint8_t cmd, size_t len, bool keep_cs)
{
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = (8 * len);   // Command is 8 bits
  t.tx_buffer = &cmd;
  t.flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0; // Keep CS active after data transfer
  return spi_device_transmit(handle, &t);
}

esp_err_t spi_read(spi_device_handle_t handle, spi_transaction_t t, uint8_t *data, size_t len, bool keep_cs)
{
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = (8 * len);           // Command is 8 bits
  t.rx_buffer = data;
  t.flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0; // Keep CS active after data transfer
  return spi_device_transmit(handle, &t);
}

esp_err_t read_manufacturer_id(spi_device_handle_t handle) {
  esp_err_t ret;

  uint8_t read_manufacturer_id_cmd = 0x90;
  uint8_t dummy_byte = 0x00;
  uint8_t ids[2];

  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;
  // Send the read manufacturer ID command
  ret = spi_cmd(handle, t, read_manufacturer_id_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read manufacturer ID command: %d", ret);
    return ret;
  }

  // Dummy cycles
  ret = spi_cmd(handle, t, dummy_byte, 3, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending dummy cycles: %d", ret);
    return ret;
  }

  // Read the ID
  ret = spi_read(handle, t, ids, 2, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading manufacturer ID: %d", ret);
    return ret;
  }

  spi_device_release_bus(handle);

  // Print the ID
  ESP_LOGI(TAG, "Manufacturer ID: %02X, Device ID: %02X", ids[0], ids[1]);
  return ESP_OK;
}

esp_err_t read_unique_id(spi_device_handle_t handle)
{
  esp_err_t ret;

  uint8_t cmd = 0x4B;
  uint8_t dummy_byte = 0x00;
  uint8_t id[8];

  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;
  // Send the read ID command
  ret = spi_cmd(handle, t, cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read ID command: %d", ret);
    return ret;
  }

  // Dummy cycles
  ret = spi_cmd(handle, t, dummy_byte, 4, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending dummy cycles: %d", ret);
    return ret;
  }

  // Read the ID
  ret = spi_read(handle, t, id, 8, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading unique ID: %d", ret);
    return ret;
  }

  spi_device_release_bus(handle);

  // Print the ID
  ESP_LOGI(TAG, "ID: %02X %02X %02X %02X %02X %02X %02X %02X", id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7]);
  return ESP_OK;
}

void init_littlefs() {
  
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

  ret = read_unique_id(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }

  ret = read_manufacturer_id(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }

  // init littleFS
  init_littlefs();

  return ESP_OK;
}
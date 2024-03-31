#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "w25q128.h"

static const char *TAG = "W25Q128";

esp_err_t spi_write(spi_device_handle_t handle, spi_transaction_t t, const void *data, size_t len, bool keep_cs)
{
  memset(&t, 0, sizeof(t));       // Zero out the transaction
  t.length = (8 * len);           // Command is 8 bits
  t.tx_buffer = data;
  t.flags = keep_cs ? SPI_TRANS_CS_KEEP_ACTIVE : 0; // Keep CS active after data transfer
  // print data and length and CS
  // printf("---- SPI WRITE ----\n");
  // printf("Data: ");
  // for(int i = 0; i < len; i++) {
  //   printf("%02X ", ((uint8_t *)data)[i]);
  // }
  // printf("\n");
  // printf("Length: %d\n", len);
  // printf("CS: %d\n", keep_cs);
  // printf("--------------------\n");
  return spi_device_transmit(handle, &t);
}

esp_err_t spi_read(spi_device_handle_t handle, spi_transaction_t t, void *data, size_t len, bool keep_cs)
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
  ret = spi_write(handle, t, &read_manufacturer_id_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read manufacturer ID command: %d", ret);
    return ret;
  }

  // Dummy cycles
  ret = spi_write(handle, t, &dummy_byte, 3, true);
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
  ret = spi_write(handle, t, &cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read ID command: %d", ret);
    return ret;
  }

  // Dummy cycles
  ret = spi_write(handle, t, &dummy_byte, 4, true);
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

esp_err_t w25q128_read_status_reg_1(spi_device_handle_t handle, spi_transaction_t t, uint8_t *status_reg) {
  esp_err_t ret;

  // Send the read status register command
  uint8_t read_status_reg_cmd = W25Q128_CMD_READ_S1;
  ret = spi_write(handle, t, &read_status_reg_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read status register command %02X: %d", W25Q128_CMD_READ_S1, ret);
    return ESP_FAIL;
  }

  ret = spi_read(handle, t, status_reg, 1, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading status register: %d", ret);
    return ESP_FAIL;
  }
  return ESP_OK;
}

int w25q128_is_write_enabled(spi_device_handle_t handle, spi_transaction_t t) {
  esp_err_t ret;

  uint8_t status_reg;
  ret = w25q128_read_status_reg_1(handle, t, &status_reg);
  if(ret != ESP_OK) {
    return -1;
  }

  // print the status register
  ESP_LOGI(TAG, "WE Status Register: %02X", status_reg);

  return status_reg & W25Q128_WRITE_ENABLE_LATCH_BIT;
}

int w25q128_write_is_in_progress(spi_device_handle_t handle, spi_transaction_t t) {
  esp_err_t ret;

  uint8_t status_reg;
  ret = w25q128_read_status_reg_1(handle, t, &status_reg);
  if(ret != ESP_OK) {
    return -1;
  }

  // print the status register
  ESP_LOGI(TAG, "WIP Status Register: %02X", status_reg);

  return status_reg & W25Q128_WRITE_IN_PROGRESS_BIT;
}


esp_err_t w25q128_write_enable(spi_device_handle_t handle, spi_transaction_t t) {
  esp_err_t ret;

  uint8_t write_en_cmd = W25Q128_CMD_WRITE_ENABLE;
  // Send the write enable command
  ret = spi_write(handle, t, &write_en_cmd, 1, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending write enable command %02X: %d", W25Q128_CMD_WRITE_ENABLE, ret);
    return ESP_FAIL;
  }

  // check that the write enable latch is set
  if(!w25q128_is_write_enabled(handle, t)) {
    ESP_LOGE(TAG, "Write enable latch not set");
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t w25q128_chip_erase(spi_device_handle_t handle) {
  esp_err_t ret;

  spi_device_acquire_bus(handle, portMAX_DELAY);

  spi_transaction_t t;

  ret = w25q128_write_enable(handle, t);
    if(ret != ESP_OK) {
      ESP_LOGE(TAG, "Error during write enable in erase: %d", ret);
      return -1;
    }

  // Send the erase command
  uint8_t erase_cmd = W25Q128_CMD_CHIP_ERASE;
  ret = spi_write(handle, t, &erase_cmd, 1, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending chip erase command %02X: %d", W25Q128_CMD_CHIP_ERASE, ret);
  }

  // wait for the write to complete
  while(w25q128_write_is_in_progress(handle, t)) {
    ESP_LOGI(TAG, "Waiting for erase to complete");
    // delay for 5 seconds
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }

  spi_device_release_bus(handle);

  return ESP_OK;
}

esp_err_t w25q128_read_data(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr, void *data, size_t len) {
  esp_err_t ret;

  // Send the read command
  uint8_t read_data_cmd = W25Q128_CMD_READ_DATA;
  ret = spi_write(handle, t, &read_data_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending read data command %02X: %d", W25Q128_CMD_READ_DATA, ret);
    return ESP_FAIL;
  }

  // Send the address
  // convert the address to a 3 byte array
  uint8_t addr_buf[3];
  addr_buf[0] = (addr >> 16) & 0xFF;
  addr_buf[1] = (addr >> 8) & 0xFF;
  addr_buf[2] = addr & 0xFF;

  ret = spi_write(handle, t, addr_buf, 3, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending address: %d", ret);
    return ESP_FAIL;
  }

  // Read the data
  ret = spi_read(handle, t, data, len, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading data: %d", ret);
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t w25q128_write_data(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr, const void *data, size_t len) {
  esp_err_t ret;

  ESP_LOGI(TAG, "Waiting for previous write to complete. This may take a while...");
  while(w25q128_write_is_in_progress(handle, t)) {
    // delay for 10 ms
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  ret = w25q128_write_enable(handle, t);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error during write enable in write_test: %d", ret);
    return ret;
  }

  // send the instruction
  uint8_t write_cmd = W25Q128_CMD_PROGRAM_PAGE;
  ret = spi_write(handle, t, &write_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending write data command %02X: %d", W25Q128_CMD_PROGRAM_PAGE, ret);
    spi_device_release_bus(handle);
    return ret;
  }

  // Send the address
  // convert the address to a 3 byte array
  uint8_t addr_buf[3];
  addr_buf[0] = (addr >> 16) & 0xFF;
  addr_buf[1] = (addr >> 8) & 0xFF;
  addr_buf[2] = addr & 0xFF;

  ret = spi_write(handle, t, addr_buf, 3, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending address: %d", ret);
    spi_device_release_bus(handle);
    return ret;
  }

  // Write the data
  ret = spi_write(handle, t, data, len, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing data: %d", ret);
    spi_device_release_bus(handle);
    return ret;
  }

  // wait for the write to complete
  while(w25q128_write_is_in_progress(handle, t)) {
    ESP_LOGI(TAG, "Waiting for write to complete");
    // delay for 10 ms
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  return ESP_OK;
}

esp_err_t w25q128_sector_erase(spi_device_handle_t handle, spi_transaction_t t, uint32_t addr) {
  esp_err_t ret;

  ESP_LOGI(TAG, "Waiting for previous write to complete. This may take a while...");
  while(w25q128_write_is_in_progress(handle, t)) {
    // delay for 10 ms
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  ret = w25q128_write_enable(handle, t);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error during write enable in erase: %d", ret);
    return -1;
  }

  // Send the erase command
  uint8_t erase_cmd = W25Q128_CMD_SECTOR_ERASE;
  ret = spi_write(handle, t, &erase_cmd, 1, true);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending block erase command %02X: %d", W25Q128_CMD_SECTOR_ERASE, ret);
  }

  // Send the address
  // convert the address to a 3 byte array
  uint8_t addr_buf[3];
  addr_buf[0] = (addr >> 16) & 0xFF;
  addr_buf[1] = (addr >> 8) & 0xFF;
  addr_buf[2] = addr & 0xFF;
  ret = spi_write(handle, t, addr_buf, 3, false);
  if(ret != ESP_OK) {
    ESP_LOGE(TAG, "Error sending address: %d", ret);
    spi_device_release_bus(handle);
    return -1;
  }

  // wait for the write to complete
  while(w25q128_write_is_in_progress(handle, t)) {
    ESP_LOGI(TAG, "Waiting for erase to complete");
    // delay for 10 ms
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  return ESP_OK;
}

esp_err_t w25q128_init(spi_device_handle_t handle) {
  // Initialize SPI
  esp_err_t ret;

  ret = read_unique_id(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }

  ret = read_manufacturer_id(handle);
  if(ret != ESP_OK) {
    return ESP_FAIL;
  }

  // Chip erase can take 40 - 200 seconds;
  // ret = w25q128_chip_erase(handle);
  // if(ret != ESP_OK) {
  //   ESP_LOGI(TAG, "FAILED TO ERASE CHIP");
  // }

  return ESP_OK;
}
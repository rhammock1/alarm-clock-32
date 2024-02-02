#include "esp_log.h"
#include "ds1307.h"
#include "utils/errors.h"

static const char *TAG = "DS1307";

/**
 * @brief Read a sequence of bytes from a ds1307_ sensor registers
 */
static esp_err_t ds1307_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, DS1307_SENSOR_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a byte to a ds1307_ sensor register
 */
static esp_err_t ds1307__register_write_byte(uint8_t reg_addr, uint8_t data)
{
  int ret;
  uint8_t write_buf[2] = {reg_addr, data};

  ret = i2c_master_write_to_device(I2C_MASTER_NUM, DS1307_SENSOR_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

  return ret;
}

esp_err_t ds1307_init() {
  ESP_LOGI(TAG, "Initializing DS1307..");
  return ESP_OK;
}
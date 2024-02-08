#include <time.h>
#include <string.h>
#include "esp_log.h"
#include "ds1307.h"
#include "errors.h"

static const char *TAG = "DS1307";

/**
 * @brief Read a sequence of bytes from a ds1307_ sensor registers
 */
static esp_err_t ds1307_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, DS1307_SENSOR_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

uint8_t dec_to_bcd(uint8_t val)
{
  return ((val / 10) << 4) + (val % 10);
}

uint8_t bcd_to_dec(uint8_t val)
{
  return (val >> 4) * 10 + (val & 0x0f);
}

esp_err_t ds1307_set_time(struct tm *timeinfo) {
  uint8_t write_buf[8] = {
    0x00,
    dec_to_bcd(timeinfo->tm_sec), // Seconds
    dec_to_bcd(timeinfo->tm_min), // Minutes
    dec_to_bcd(timeinfo->tm_hour), // Hours
    dec_to_bcd(timeinfo->tm_wday + 1), // Day of week
    dec_to_bcd(timeinfo->tm_mday), // Day
    dec_to_bcd(timeinfo->tm_mon + 1), // Month
    dec_to_bcd(timeinfo->tm_year - 2000) // Year
  };

  esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, DS1307_SENSOR_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error setting time: %d", ret);
    return ret;
  }
  return ESP_OK;
}

esp_err_t ds1307_read_time(struct tm *timeinfo) {
  uint8_t read_buf[7];

  esp_err_t ret = ds1307_register_read(0x00, read_buf, sizeof(read_buf));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading time: %d", ret);
    return ret;
  }

  timeinfo->tm_sec = bcd_to_dec(read_buf[0] & 0x7F);
  timeinfo->tm_min = bcd_to_dec(read_buf[1]);
  timeinfo->tm_hour = bcd_to_dec(read_buf[2] & 0x1f);
  if(read_buf[2] & 0x20) {
    timeinfo->tm_hour += 12;
  }
  timeinfo->tm_wday = bcd_to_dec(read_buf[3]) - 1;
  timeinfo->tm_mday = bcd_to_dec(read_buf[4]);
  timeinfo->tm_mon = bcd_to_dec(read_buf[5]) - 1;
  timeinfo->tm_year = bcd_to_dec(read_buf[6]) + 2000;

  return ESP_OK;
}

esp_err_t ds1307_init() {
  ESP_LOGI(TAG, "Initializing DS1307..");

  struct tm timeinfo;

  char month_str[4];
  sscanf(__DATE__, "%s %d %d", month_str, &timeinfo.tm_mday, &timeinfo.tm_year);

  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int month = 0;
  for (int i = 0; i < 12; i++)
  {
    if (strcmp(month_str, months[i]) == 0)
    {
      month = i + 1;
      break;
    }
  }
  timeinfo.tm_mon = month;

  ESP_LOGI(TAG, "This program was compiled on %s at %s\n", __DATE__, __TIME__);
  sscanf(__TIME__, "%d:%d:%d", &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);

  // Set the time
  if(false) {
    esp_err_t ret = ds1307_set_time(&timeinfo);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error setting time: %d", ret);
      error_blink_task(SOURCE_DS1307);
    }
  }

  ESP_LOGI(TAG, "DS1307 initialized successfully");

  struct tm timeinfo_read;
  ds1307_read_time(&timeinfo_read);

  ESP_LOGI(TAG, "Time read from DS1307: %d-%d-%d %d:%d:%d",
    timeinfo_read.tm_year, timeinfo_read.tm_mon, timeinfo_read.tm_mday,
    timeinfo_read.tm_hour, timeinfo_read.tm_min, timeinfo_read.tm_sec);

  return ESP_OK;
}
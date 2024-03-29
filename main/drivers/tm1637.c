#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "tm1637.h"
#include "errors.h"

static const char *TAG = "TM1637";

SemaphoreHandle_t tm1637_mux;

void configure_gpio() {
  // Set the CLK and DIO pins as output
  gpio_set_direction(TM1637_CLK_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(TM1637_CLK_PIN, 0);
  ets_delay_us(2);

  gpio_set_direction(TM1637_DIO_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(TM1637_DIO_PIN, 0);
  ets_delay_us(2);

  gpio_set_level(TM1637_CLK_PIN, 1);
  ets_delay_us(2);
}

void tm1637_write_byte(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    gpio_set_level(TM1637_CLK_PIN, 0);
    ets_delay_us(3);
    // Set data bit
    gpio_set_level(TM1637_DIO_PIN, data & 0x01);

    data >>= 1;
    ets_delay_us(3);
    gpio_set_level(TM1637_CLK_PIN, 1);
    ets_delay_us(3);
  }

  // Wait for acknowledge
  gpio_set_direction(TM1637_DIO_PIN, GPIO_MODE_INPUT);
  // CLK to zero
  gpio_set_level(TM1637_CLK_PIN, 0); // TM1637 starts ACK (pulls DIO low)
  ets_delay_us(5);
  // CLK to high
  gpio_set_level(TM1637_CLK_PIN, 1);
  ets_delay_us(5);
  gpio_set_level(TM1637_CLK_PIN, 0); // TM1637 ends ACK (releasing DIO)
  ets_delay_us(5);
  gpio_set_direction(TM1637_DIO_PIN, GPIO_MODE_OUTPUT);
}

void tm1637_start()
{
  // Start signal
  gpio_set_level(TM1637_DIO_PIN, 0);
  ets_delay_us(2);
}

void tm1637_stop() {
  gpio_set_level(TM1637_DIO_PIN, 0);
  ets_delay_us(2);
  gpio_set_level(TM1637_CLK_PIN, 1);
  ets_delay_us(2);
  gpio_set_level(TM1637_DIO_PIN, 1);
  ets_delay_us(2);
}

esp_err_t tm1637_set_brightness(uint8_t brightness) {
  if (xSemaphoreTake(tm1637_mux, MAX_BLOCK) != pdTRUE)
  {
    ESP_LOGE(TAG, "Could not take tm1637_mux");
    return ESP_FAIL;
  }
  // Set the brightness of the display
  uint8_t cmd = 0x88 + brightness;
  tm1637_start();
  tm1637_write_byte(cmd);
  tm1637_stop();

  xSemaphoreGive(tm1637_mux);
  return ESP_OK;
}

static const int8_t tm1637_symbols[] = {
    // XGFEDCBA
    0x3f, // 0b00111111,    // 0
    0x06, // 0b00000110,    // 1
    0x5b, // 0b01011011,    // 2
    0x4f, // 0b01001111,    // 3
    0x66, // 0b01100110,    // 4
    0x6d, // 0b01101101,    // 5
    0x7d, // 0b01111101,    // 6
    0x07, // 0b00000111,    // 7
    0x7f, // 0b01111111,    // 8
    0x6f, // 0b01101111,    // 9
};

void tm1637_set_segment_raw(const uint8_t segment_idx, const uint8_t data)
{
  tm1637_start();
  tm1637_write_byte(TM1637_ADDR_FIXED);
  tm1637_stop();
  tm1637_start();
  tm1637_write_byte(segment_idx | 0xc0); // selects the segment to write to
  tm1637_write_byte(data);
  tm1637_stop();

  // tm1637_set_brightness(1);
}

void tm1637_set_segment_number(const uint8_t segment_idx, const uint8_t num)
{
  uint8_t seg_data = 0x00;

  if (num < (sizeof(tm1637_symbols) / sizeof(tm1637_symbols[0])))
  {
    seg_data = tm1637_symbols[num]; // Select proper segment image
  }

  if(segment_idx == 1)
  {
    seg_data |= 0x80; // Set the decimal point (colon)
  }

  tm1637_set_segment_raw(segment_idx, seg_data);
}

esp_err_t tm1637_update_time(uint8_t hours, uint8_t minutes){
  // ESP_LOGI(TAG, "Updating time: %d:%d", hours, minutes);

  if (xSemaphoreTake(tm1637_mux, MAX_BLOCK) != pdTRUE)
  {
    ESP_LOGE(TAG, "Could not take tm1637_mux");
    return ESP_FAIL;
  }

  bool pm = hours > 12;
  // Convert hour to 12 hour format
  if (pm) {
    hours -= 12;
  }

  if(hours == 0) {
    hours = 12;
  }

  // Write hours and minutes to the display
  tm1637_start();
  if(hours < 10) {
    tm1637_set_segment_number(0, 0);
    tm1637_set_segment_number(1, hours);
  } else {
    tm1637_set_segment_number(0, hours / 10);
    tm1637_set_segment_number(1, hours % 10);
  }

  if(minutes < 10) {
    tm1637_set_segment_number(2, 0);
    tm1637_set_segment_number(3, minutes);
  } else {
    tm1637_set_segment_number(2, minutes / 10);
    tm1637_set_segment_number(3, minutes % 10);
  }
  tm1637_stop();

  xSemaphoreGive(tm1637_mux);
  return ESP_OK;
}

esp_err_t tm1637_init() {
  ESP_LOGI(TAG, "Initializing TM1637..");
  tm1637_mux = xSemaphoreCreateMutex();
  configure_gpio();

  if (xSemaphoreTake(tm1637_mux, MAX_BLOCK) != pdTRUE)
  {
    ESP_LOGE(TAG, "Could not take tm1637_mux");
    return ESP_FAIL;
  }

  tm1637_start();
  tm1637_write_byte(0x40);
  tm1637_stop();
  ESP_LOGI(TAG, "Set to write display register");


  tm1637_start();
  tm1637_write_byte(0xC0);
  ESP_LOGI(TAG, "Set the first address");
  for (uint8_t i = 0; i < 6; i++) {
    // Clear the display
    tm1637_write_byte(0x00);
  }
  tm1637_stop();

  xSemaphoreGive(tm1637_mux);

  // Set the brightness to 7
  tm1637_set_brightness(1);

  ESP_LOGI(TAG, "TM1637 initialized successfully");
  return ESP_OK;
}

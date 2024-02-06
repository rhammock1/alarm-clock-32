#include "esp_log.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "tm1637.h"
#include "utils/errors.h"

static const char *TAG = "TM1637";

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

void tm1637_set_brightness(uint8_t brightness) {
  // Set the brightness of the display
  uint8_t cmd = 0x88 + brightness;
  tm1637_start();
  tm1637_write_byte(cmd);
  tm1637_stop();
}

void tm1637_update_time(uint8_t hours, uint8_t minutes) {
  // Write hours and minutes to the display
  tm1637_start();
  tm1637_write_byte(hours);
  tm1637_write_byte(minutes);
  tm1637_stop();
}

void tm1637_init() {
  ESP_LOGI(TAG, "Initializing TM1637..");
  configure_gpio();

  tm1637_start();
  tm1637_write_byte(0x40);
  tm1637_stop();
  ESP_LOGI(TAG, "Set to write display register");


  tm1637_start();
  tm1637_write_byte(0xC0);
  ESP_LOGI(TAG, "Set the first address");
  for (uint8_t i = 0; i < 6; i++) {
    tm1637_write_byte(0xFF);
  }
  tm1637_stop();

  tm1637_start();
  tm1637_write_byte(0x8F);
  tm1637_stop();

  // // Set the brightness to 7
  // tm1637_set_brightness(7);

  // // Update the time to 00:00
  // tm1637_update_time(0, 0);

  ESP_LOGI(TAG, "TM1637 initialized successfully");
}

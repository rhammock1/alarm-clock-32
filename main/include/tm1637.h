#include <stdio.h>

#define TM1637_CLK_PIN 18
#define TM1637_DIO_PIN 19

#define TM1637_ADDR_AUTO 0x40
#define TM1637_ADDR_FIXED 0x44
#define TM1637_SEGMENTS_ADDR 0xC0
#define TM1637_BRIGHTNESS_ADDR 0x80

esp_err_t tm1637_init(void);
esp_err_t tm1637_set_brightness(uint8_t brightness);
esp_err_t tm1637_update_time(uint8_t hours, uint8_t minutes);

#include <stdio.h>

#define TM1637_CLK_PIN 18
#define TM1637_DIO_PIN 19

#define TM1637_I2C_COMM1 0x40
#define TM1637_SEGMENTS_ADDR 0xC0
#define TM1637_BRIGHTNESS_ADDR 0x80

void tm1637_init(void);
void tm1637_set_brightness(uint8_t brightness);
void tm1637_update_time(uint8_t hours, uint8_t minutes);

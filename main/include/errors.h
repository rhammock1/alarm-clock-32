#include "stdio.h"

typedef enum {
  SOURCE_I2C = 1,
  SOURCE_VCNL4010 = 2,
  SOURCE_DS1307 = 3,
  SOURCE_TM1637 = 4,
  SOURCE_WIFI = 5,
  SOURCE_HTTP_SERVER = 6,
  SOURCE_LITTLEFS = 7,
} error_source_t;

extern int MAX_BLOCK;

void configure_error_led(void);

void error_blink_task(error_source_t source);

void blink_led_once(void);

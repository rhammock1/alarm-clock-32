#include <stdio.h>
#include "utils/i2c.h"

#define DS1307_SENSOR_ADDR 0x57

esp_err_t ds1307_init(void);

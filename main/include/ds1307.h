#include <stdio.h>
#include "utils/i2c_config.h"
#include <time.h>

#define DS1307_SENSOR_ADDR 0x68

// Registers
#define DS1307_SECONDS 0x00
#define DS1307_MINUTES 0x01
#define DS1307_HOURS 0x02
#define DS1307_DAY 0x03
#define DS1307_DATE 0x04
#define DS1307_MONTH 0x05
#define DS1307_YEAR 0x06
#define DS1307_CONTROL 0x07 // Control Register for ouput of square wave

// Even if we don't use these structs, they are useful to understand the bit layout of the registers
typedef struct {
  uint8_t low_seconds: 4;
  uint8_t high_seconds: 3;
  uint8_t clock_halt: 1;
} DS1307SecondsRegister;

typedef struct {
  uint8_t low_minutes: 4;
  uint8_t high_minutes: 3;
  uint8_t not_used : 1;
} DS1307MinutesRegister;

typedef struct {
  uint8_t low_hours: 4;
  uint8_t high_hours: 2;
  uint8_t mode: 1;
  uint8_t not_used: 1;
} DS1307HoursRegister;

typedef struct {
  uint8_t day: 3;
  uint8_t not_used: 5;
} DS1307DayRegister;

typedef struct {
  uint8_t low_date: 4;
  uint8_t high_date: 2;
  uint8_t not_used: 2;
} DS1307DateRegister;

typedef struct {
  uint8_t low_month: 4;
  uint8_t high_month: 1;
  uint8_t not_used: 3;
} DS1307MonthRegister;

typedef struct {
  uint8_t low_year: 4;
  uint8_t high_year: 4;
} DS1307YearRegister;

esp_err_t ds1307_init(void);

esp_err_t ds1307_read_time(struct tm *timeinfo);

esp_err_t ds1307_set_time(struct tm *timeinfo);

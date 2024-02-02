#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "errors.h"

// static const char *TAG = "ERROR";

#define ERROR_LED 2

void configure_error_led(void)
{
  gpio_reset_pin(ERROR_LED);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(ERROR_LED, GPIO_MODE_OUTPUT);
}

void error_blink_task(void)
{
  while (1)
  {
    gpio_set_level(ERROR_LED, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(ERROR_LED, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void blink_led_once(void) {
  gpio_set_level(ERROR_LED, 1);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(ERROR_LED, 0);
}
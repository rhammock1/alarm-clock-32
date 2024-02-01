#include <stdio.h>
#include "esp_log.h"
#include "utils/i2c.h"
#include "utils/errors.h"
#include "sensors/ds1307.h"
#include "sensors/vcnl4010.h"

static const char *TAG = "ALARM-CLOCK";

#define GPIO_INPUT_IO_23 23
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_23) | (1ULL << GPIO_INPUT_IO_23))

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  ESP_LOGI(TAG, "INTERRUPT DETECTED");
  // Query the vcnl4010 sensor's interrupt status register and do something with it
  blink_led_once();
}

void configure_interrupts(void) {
  gpio_config_t io_conf;
  io_conf.intr_type = GPIO_INTR_POSEDGE; // Interrupt on rising edge
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = 1;
  gpio_config(&io_conf);

  // Install the ISR service
  esp_err_t install_ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
  if (install_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error installing ISR service: %d", install_ret);
    error_blink_task();
  }

  // Add the ISR handler for GPIO 23
  esp_err_t add_ret = gpio_isr_handler_add(GPIO_INPUT_IO_23, gpio_isr_handler, (void *)GPIO_INPUT_IO_23);
  if (add_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error adding ISR handler: %d", add_ret);
    error_blink_task();
  }
}

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init(void)
{
  int i2c_master_port = I2C_MASTER_NUM;

  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  i2c_param_config(i2c_master_port, &conf);

  return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

void app_main(void)
{
  ESP_LOGI(TAG, "Beginning alarm clock setup..");

  configure_error_led();

  // init i2c
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_LOGI(TAG, "I2C initialized successfully");

  esp_err_t ret = vcnl4010_init();
  // This will need to change to a web notification or something
  //  
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing vcnl4010: %d", ret);
    error_blink_task();
  }

  ESP_LOGI(TAG, "VCNL4010 initialized successfully");
  configure_interrupts();

  uint16_t proximity;
  uint16_t ambient_light;
  while(1) {
    vcnl4010_readProximityResult(&proximity);
    ESP_LOGI(TAG, "Proximity: %d", proximity);
    vcnl4010_readAmbientLight(&ambient_light);
    ESP_LOGI(TAG, "AMBIENT LIGHT: %d", ambient_light);
    if(proximity > 2300 || ambient_light < 50) {
      ESP_LOGI(TAG, "Alarm!");
      blink_led_once();
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }

  // ret = ds1307_init();
  // if (ret != ESP_OK) {
  //   ESP_LOGE(TAG, "Error initializing ds1307: %d", ret);
  //   error_blink_task();
  // }
}

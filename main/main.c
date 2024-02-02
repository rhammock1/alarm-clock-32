#include <stdio.h>
#include "esp_log.h"
#include "utils/i2c.h"
#include "utils/errors.h"
#include "sensors/ds1307.h"
#include "sensors/vcnl4010.h"

static const char *TAG = "ALARM-CLOCK";

#define GPIO_INPUT_IO_23 23
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_23)

uint8_t last_interrupt_status;

TaskHandle_t handler_task_handle = NULL;

void handler_task(void *arg)
{
  while (1)
  {
    ESP_LOGI(TAG, "IN HANDLER TASK");
    // Wait for the ISR to signal us
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Now we can safely call the non-IRAM-safe functions
    uint8_t interrupt_status;
    vcnl4010_readInterruptStatus(&interrupt_status);
    // last_interrupt_status = interrupt_status;
    ESP_LOGI(TAG, "Interrupt status: %d", interrupt_status);
    vcnl4010_writeInterruptStatus(0x01);
    vcnl4010_readInterruptStatus(&interrupt_status);
    // blink_led_once();
    // last_interrupt_status = interrupt_status;
    ESP_LOGI(TAG, "Interrupt status: %d", interrupt_status);
  }
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Signal the handler task to run
  vTaskNotifyGiveFromISR(handler_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void configure_interrupts(void) {
  gpio_config_t io_conf;
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.intr_type = GPIO_INTR_NEGEDGE; // Interrupt on rising edge
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);

  // Install the ISR service
  gpio_install_isr_service(0);

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

  // Create the handler task
  xTaskCreate(
      handler_task,        // Task function
      "Handler Task",      // Name of task
      10000,               // Stack size of task
      NULL,                // Parameter of the task
      1,                   // Priority of the task
      &handler_task_handle // Task handle to keep track of created task
  );

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
  ESP_LOGI(TAG, "Interrupts configured successfully");

  uint16_t proximity;
  uint16_t ambient_light;
  while(1) {
    // vcnl4010_readProximityResult(&proximity);
    // ESP_LOGI(TAG, "Proximity: %d", proximity);
    // vcnl4010_readAmbientLight(&ambient_light);
    // ESP_LOGI(TAG, "AMBIENT LIGHT: %d", ambient_light);
    // if(proximity > 2300 || ambient_light < 50) {
    //   ESP_LOGI(TAG, "Alarm!");
    //   blink_led_once();
    // }
    if(last_interrupt_status) {
      ESP_LOGI(TAG, "Alarm! %d", last_interrupt_status);
      blink_led_once();
      last_interrupt_status = 0;
    }
    ESP_LOGI(TAG, "Waiting for interrupt");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }

  // ret = ds1307_init();
  // if (ret != ESP_OK) {
  //   ESP_LOGE(TAG, "Error initializing ds1307: %d", ret);
  //   error_blink_task();
  // }
}

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "i2c_config.h"
#include "errors.h"
#include "ds1307.h"
#include "vcnl4010.h"
#include "tm1637.h"
#include "wifi.h"
#include "http_server.h"

static const char *TAG = "ALARM-CLOCK";

#define GPIO_INPUT_IO_23 23
#define GPIO_INPUT_PIN_SEL_23 (1ULL << GPIO_INPUT_IO_23)


TaskHandle_t interrupt_23_task_handle = NULL;

void interrupt_23_handler(void *arg)
{
  while (1)
  {
    ESP_LOGI(TAG, "IN HANDLER TASK");
    // Wait for the ISR to signal us
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Now we can safely call the non-IRAM-safe functions
    uint8_t interrupt_status;
    vcnl4010_readInterruptStatus(&interrupt_status);
    ESP_LOGI(TAG, "Interrupt status: %d", interrupt_status);

    if(interrupt_status & 0x01)
    {
      ESP_LOGI(TAG, "Proximity interrupt triggered");
      blink_led_once();
      // Must write ones in each bit to clear the interrupt
      vcnl4010_writeInterruptStatus(0x01);
    }
    

    ESP_LOGI(TAG, "Interrupt status: %d", interrupt_status);
  }
}

static void IRAM_ATTR gpio_23_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  // Signal the handler task to run
  vTaskNotifyGiveFromISR(interrupt_23_task_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void create_interrupt_tasks(void)
{
  ESP_LOGI(TAG, "Creating interrupt tasks..");
  // Create the handler task for interrupt 23
  xTaskCreate(
    interrupt_23_handler, // Task function
    "Interrupt 23 Handler",       // Name of task
    10000,                // Stack size of task
    NULL,                 // Parameter of the task
    1,                    // Priority of the task
    &interrupt_23_task_handle  // Task handle to keep track of created task
  );

  // Add new tasks here
}

void configure_interrupts(void) {
  create_interrupt_tasks();

  // Configure the GPIO for the interrupt
  // If the interrupt types are the same, we can configure them all at once by changing the bit mask
  // If they are different, we need to configure them separately
  gpio_config_t io_23_conf;
  io_23_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL_23;
  io_23_conf.mode = GPIO_MODE_INPUT;
  io_23_conf.intr_type = GPIO_INTR_NEGEDGE; // Interrupt on rising edge
  io_23_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  io_23_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_23_conf);

  // Install the ISR service
  gpio_install_isr_service(0);

  // Add the ISR handler for GPIO 23
  esp_err_t add_ret = gpio_isr_handler_add(GPIO_INPUT_IO_23, gpio_23_isr_handler, (void *)GPIO_INPUT_IO_23);
  if (add_ret != ESP_OK) {
    ESP_LOGE(TAG, "Error adding ISR handler: %d", add_ret);
    error_blink_task(SOURCE_I2C);
  }

  ESP_LOGI(TAG, "Interrupts configured successfully");
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

void init_i2c_sensors(void)
{
  ESP_LOGI(TAG, "Initializing i2c sensors..");
  esp_err_t ret = vcnl4010_init();
  // This will need to change to a web notification or something
  //
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error initializing VCNL4010: %d", ret);
    error_blink_task(SOURCE_I2C);
  }
  ESP_LOGI(TAG, "VCNL4010 initialized successfully");

  ret = ds1307_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing DS1307: %d", ret);
    error_blink_task(SOURCE_I2C);
  }
  ESP_LOGI(TAG, "DS1307 initialized successfully");
}

void init_other_drivers() {
  ESP_LOGI(TAG, "Initializing other drivers..");
  tm1637_init();
  ESP_LOGI(TAG, "TM1637 initialized successfully");
}

void init_wifi_and_serve() {
  ESP_LOGI(TAG, "Initializing wifi and server..");
  wifi_init_sta();
  ESP_LOGI(TAG, "Wifi initialized successfully");

  esp_err_t ret = init_http_server();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing HTTP server: %d", ret);
    error_blink_task(SOURCE_WIFI);
  }
  ESP_LOGI(TAG, "HTTP server initialized successfully");
}

void app_main(void)
{
  ESP_LOGI(TAG, "Beginning alarm clock setup..");

  // init i2c
  ESP_ERROR_CHECK(i2c_master_init());
  ESP_LOGI(TAG, "I2C initialized successfully");

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  configure_error_led();
  init_i2c_sensors();
  init_other_drivers();
  configure_interrupts();

  // Start the wifi [BLOCKS REST OF CODE]
  init_wifi_and_serve();

  struct tm timeinfo;
  while(1) {
    ESP_LOGI(TAG, "Waiting for interrupt");

    esp_err_t ret = ds1307_read_time(&timeinfo);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error reading time: %d", ret);
      error_blink_task(SOURCE_DS1307);
    }
    ESP_LOGI(TAG, "Time read from DS1307: %d-%d-%d %d:%d:%d",
      timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    tm1637_update_time(timeinfo.tm_hour, timeinfo.tm_min);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

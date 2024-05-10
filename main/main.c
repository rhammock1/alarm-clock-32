#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "i2c_config.h"
#include "errors.h"
#include "ds1307.h"
#include "vcnl4010.h"
#include "tm1637.h"
#include "w25q128.h"
#include "wifi.h"
#include "lilfs.h"
#include "http_server.h"

static const char *TAG = "ALARM-CLOCK";

#define GPIO_INPUT_IO_23 23
#define GPIO_INPUT_PIN_SEL_23 (1ULL << GPIO_INPUT_IO_23)


TaskHandle_t vcnl4010_interrupt_handle = NULL;

void vcnl4010_interrupt_handler(void *arg)
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
  vTaskNotifyGiveFromISR(vcnl4010_interrupt_handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void create_interrupt_tasks(void)
{
  ESP_LOGI(TAG, "Creating interrupt tasks..");
  // Create the handler task for interrupt 23 (VCNL4010 interrupt)
  xTaskCreate(
    vcnl4010_interrupt_handler, // Task function
    "VCNL4010 Interrupt",       // Name of task
    2048,                // Stack size of task
    NULL,                 // Parameter of the task
    1,                    // Priority of the task
    &vcnl4010_interrupt_handle  // Task handle to keep track of created task
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

void init_drivers(spi_device_handle_t w25q128_handle) {
  esp_err_t ret;

  ESP_LOGI(TAG, "Initializing i2c sensors..");
  ret = vcnl4010_init();
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
  
  ret = tm1637_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error initializing TM1637: %d", ret);
    error_blink_task(SOURCE_I2C);
  }
  ESP_LOGI(TAG, "TM1637 initialized successfully");
  
  ret = w25q128_init(w25q128_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error initializing W25Q128: %d", ret);
    error_blink_task(SOURCE_I2C);
  }
  ESP_LOGI(TAG, "W25Q128 initialized successfully");
}

void init_wifi_and_serve(void *arg) {
  ESP_LOGI(TAG, "Initializing wifi and server..");
  wifi_init_sta();
  ESP_LOGI(TAG, "Wifi initialized successfully");

  esp_err_t ret = init_http_server();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing HTTP server: %d", ret);
    error_blink_task(SOURCE_WIFI);
  }
  ESP_LOGI(TAG, "HTTP server initialized successfully");
  vTaskDelete(NULL);
}

void init_littlefs_task(void *pvParameter) {
  ESP_LOGI(TAG, "Initializing LittleFS..");
  spi_device_handle_t spi_handle = *(spi_device_handle_t *)pvParameter;
  esp_err_t ret = init_littlefs(spi_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing LittleFS: %d", ret);
    error_blink_task(SOURCE_LITTLEFS);
  }
  ESP_LOGI(TAG, "LittleFS initialized successfully");
  vTaskDelete(NULL);
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

  spi_device_handle_t w25q128_handle;
    // Initialize the SPI bus
  spi_bus_config_t buscfg = {
    .miso_io_num = PIN_NUM_MISO,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1};
  ret = spi_bus_initialize(HOST, &buscfg, DMA_CHAN);
  assert(ret == ESP_OK);

  // Initialize the SPI device
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000, // Clock out at 10 MHz
    .mode = 0,                          // SPI mode 0
    .spics_io_num = PIN_NUM_CS,         // CS pin
    .queue_size = 7,                    // We want to be able to queue 7 transactions at a time
  };
  ret = spi_bus_add_device(HOST, &devcfg, &w25q128_handle);
  assert(ret == ESP_OK);

  init_drivers(w25q128_handle);
  configure_interrupts();
  
  TaskHandle_t lilfs_task_handle;
  xTaskCreate(init_littlefs_task, "LittleFSInitTask", 5120, &w25q128_handle, 1, &lilfs_task_handle);

  TaskHandle_t wifi_task_handle;
  xTaskCreate(init_wifi_and_serve, "WiFiInitTask", 4096, NULL, 1, &wifi_task_handle);

  struct tm timeinfo;
  while(1) {
    esp_err_t ret = ds1307_read_time(&timeinfo);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error reading time: %d", ret);
      error_blink_task(SOURCE_DS1307);
    }

    tm1637_update_time(timeinfo.tm_hour, timeinfo.tm_min);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

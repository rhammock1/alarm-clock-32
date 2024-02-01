#include "esp_log.h"
#include "vcnl4010.h"
#include "utils/errors.h"

static const char *TAG = "VCNL4010";

/**
 * @brief Read a sequence of bytes from a vcnl4010 sensor registers
 */
static esp_err_t
vcnl4010_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
  return i2c_master_write_read_device(I2C_MASTER_NUM, VCNL4010_SENSOR_ADDR, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a byte to a vcnl4010 sensor register
 */
static esp_err_t vcnl4010_register_write_byte(uint8_t reg_addr, uint8_t data)
{
  int ret;
  uint8_t write_buf[2] = {reg_addr, data};

  ret = i2c_master_write_to_device(I2C_MASTER_NUM, VCNL4010_SENSOR_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

  return ret;
}

void print_VCNL4010CommandRegister(VCNL4010CommandRegister commandRegister)
{
  ESP_LOGI(TAG, "Config lock: %d", commandRegister.config_lock);
  ESP_LOGI(TAG, "Ambient light data ready: %d", commandRegister.a_light_dr);
  ESP_LOGI(TAG, "Proximity data ready: %d", commandRegister.prox_data_rdy);
  ESP_LOGI(TAG, "Ambient light ON DEMAND output: %d", commandRegister.a_light_od);
  ESP_LOGI(TAG, "Proximity ON DEMAND output: %d", commandRegister.prox_od);
  ESP_LOGI(TAG, "Ambient light enable: %d", commandRegister.a_light_en);
  ESP_LOGI(TAG, "Proximity enable: %d", commandRegister.prox_en);
  ESP_LOGI(TAG, "Self timed enable: %d", commandRegister.self_timed_en);
}

void print_VCNL4010ProductIdRegister(VCNL4010ProductId productIdRegister)
{
  ESP_LOGI(TAG, "Product ID: %d", productIdRegister.product_id);
  ESP_LOGI(TAG, "Revision ID: %d", productIdRegister.revision_id);
}

void print_VCNL4010InterruptControlRegister(VCNL4010InterruptControlRegister interruptControlRegister) {
  ESP_LOGI(TAG, "Interrupt threshold select: %d", interruptControlRegister.int_thresh_sel);
  ESP_LOGI(TAG, "Interrupt threshold enable: %d", interruptControlRegister.int_thresh_en);
  ESP_LOGI(TAG, "Interrupt ambient light data ready enable: %d", interruptControlRegister.int_als_ready_en);
  ESP_LOGI(TAG, "Interrupt proximity data ready enable: %d", interruptControlRegister.int_prox_read_en);
  ESP_LOGI(TAG, "Not available: %d", interruptControlRegister.not_available);
  ESP_LOGI(TAG, "Count exceed: %d", interruptControlRegister.count_exceed);
}

void vcnl4010_readThresholdRegistersAndPrint(void) {
  uint8_t low_threshold[2];
  uint8_t high_threshold[2];

  esp_err_t ret = vcnl4010_register_read(VCNL4010_LOWTHRESHOLD1, &low_threshold[1], 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading low threshold register: %d", ret);
    error_blink_task();
  }
  ret = vcnl4010_register_read(VCNL4010_LOWTHRESHOLD0, &low_threshold[0], 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading low threshold register: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_read(VCNL4010_HITHRESHOLD1, &high_threshold[1], 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading high threshold register: %d", ret);
    error_blink_task();
  }
  ret = vcnl4010_register_read(VCNL4010_HITHRESHOLD0, &high_threshold[0], 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading high threshold register: %d", ret);
    error_blink_task();
  }

  uint16_t low_threshold_value = (low_threshold[1] << 8) | low_threshold[0];
  uint16_t high_threshold_value = (high_threshold[1] << 8) | high_threshold[0];

  ESP_LOGI(TAG, "Low threshold: %d", low_threshold_value);
  ESP_LOGI(TAG, "High threshold: %d", high_threshold_value);
}

void vcnl4010_writeProximityRate(uint8_t rate) {
  esp_err_t ret = vcnl4010_register_write_byte(VCNL4010_PROXRATE, rate);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing proximity rate: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_readProximityRate(uint8_t *rate) {
  esp_err_t ret = vcnl4010_register_read(VCNL4010_PROXRATE, rate, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading proximity rate: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_readAmbientLight(uint16_t *ambient_light) {
  uint8_t low_byte;
  uint8_t high_byte;
  esp_err_t ret = vcnl4010_register_read(VCNL4010_AMBIENTRESULT1, &high_byte, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading ambient light register 1: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_read(VCNL4010_AMBIENTRESULT0, &low_byte, 1);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error reading ambient light register 0: %d", ret);
    error_blink_task();
  }

  // Print the low byte and high byte
  ESP_LOGI(TAG, "LIGHT Low byte: %d", low_byte);
  ESP_LOGI(TAG, "LIGHT High byte: %d", high_byte);

  *ambient_light = (high_byte << 8) | low_byte;
}


void vcnl4010_readCommandRegister(uint8_t *command_register) {
  esp_err_t ret = vcnl4010_register_read(VCNL4010_COMMAND, command_register, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading command register: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_readProximityResult(uint16_t *proximity_result) {
  uint8_t low_byte;
  uint8_t high_byte;

  esp_err_t ret = vcnl4010_register_read(VCNL4010_PROXIMITYRESULT1, &high_byte, 1);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error reading proximity result register 1: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_read(VCNL4010_PROXIMITYRESULT0, &low_byte, 1);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error reading proximity result register 0: %d", ret);
    error_blink_task();
  }

  // Print the low byte and high byte
  ESP_LOGI(TAG, "PROX Low byte: %d", low_byte);
  ESP_LOGI(TAG, "PROX High byte: %d", high_byte);

  *proximity_result = (high_byte << 8) | low_byte;
}
void vcnl4010_writeCommandRegister(VCNL4010CommandRegister commandRegister) {
  // convert the struct to a byte
  uint8_t byte = *(uint8_t*)&commandRegister;
  // (commandRegister.config_lock << 7) | (commandRegister.a_light_dr << 6) | (commandRegister.prox_data_rdy << 5) | (commandRegister.a_light_od << 4) | (commandRegister.prox_od << 3) | (commandRegister.a_light_en << 2) | (commandRegister.prox_en << 1) | (commandRegister.self_timed_en);
  ESP_LOGI(TAG, "Command register before write: %d", byte);
  esp_err_t ret = vcnl4010_register_write_byte(VCNL4010_COMMAND, byte);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing command register: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_writeInterruptControlRegister(VCNL4010InterruptControlRegister interruptControlRegister) {
  uint8_t byte = *(uint8_t*)&interruptControlRegister;
  // Print the byte in binary MSB first
  ESP_LOGI(TAG, "Interrupt control register before write: %d", byte);
  ESP_LOGI(TAG, "BINARY OF BYTE: %d%d%d%d%d%d%d%d", (byte & 0x80) >> 7, (byte & 0x40) >> 6, (byte & 0x20) >> 5, (byte & 0x10) >> 4, (byte & 0x08) >> 3, (byte & 0x04) >> 2, (byte & 0x02) >> 1, byte & 0x01);
  esp_err_t ret = vcnl4010_register_write_byte(VCNL4010_INTCONTROL, byte);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing interrupt control register: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_readInterruptControlRegister(uint8_t *interrupt_control_register) {
  esp_err_t ret = vcnl4010_register_read(VCNL4010_INTCONTROL, interrupt_control_register, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading interrupt control register: %d", ret);
    error_blink_task();
  }
}

void vcnl4010_writeThresholdRegisters(uint8_t reg_addr, uint16_t threshold) {
  uint8_t low_byte = threshold & 0xFF;
  uint8_t high_byte = (threshold >> 8) & 0xFF;

  esp_err_t ret = vcnl4010_register_write_byte(reg_addr, high_byte);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing low byte to threshold register: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_write_byte(reg_addr + 1, low_byte);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing high byte to threshold register: %d", ret);
    error_blink_task();
  }
}

esp_err_t vcnl4010_init() {
  ESP_LOGI(TAG, "Initializing vcnl4010 sensor..");
  esp_err_t ret = vcnl4010_register_write_byte(VCNL4010_IRLED, 0x0A);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing IR LED current: %d", ret);
    error_blink_task();
  }

  ESP_LOGI(TAG, "Reading product ID register..");
  ret = vcnl4010_register_write_byte(VCNL4010_PROXRATE, 0x02);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing proximity rate: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_write_byte(VCNL4010_AMBIENTPARAMETER, 0x1D);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error writing ambient light parameter: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_write_byte(VCNL4010_INTCONTROL, 0x42);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Error writing interrupt control: %d", ret);
    error_blink_task();
  }

  ret = vcnl4010_register_write_byte(VCNL4010_INTCONTROL, 0x42);
  vcnl4010_writeThresholdRegisters(VCNL4010_LOWTHRESHOLD1, 0x0000);
  vcnl4010_writeThresholdRegisters(VCNL4010_HITHRESHOLD1, 0x16E4);

  ret = vcnl4010_register_write_byte(VCNL4010_COMMAND, 0x07); 

  // wait before taking measurement
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // Read from the interupt status register
  uint8_t interrupt_status;
  ret = vcnl4010_register_read(VCNL4010_INTERRUPTSTATUS, &interrupt_status, 1);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Error reading interrupt status register: %d", ret);
    error_blink_task();
  }
  ESP_LOGI(TAG, "Interrupt status register: %d", interrupt_status);

  return ESP_OK;
}

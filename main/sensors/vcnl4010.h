#include <stdio.h>
#include "utils/i2c.h"

#define VCNL4010_SENSOR_ADDR 0x13

// VCNL4010 Register Map
#define VCNL4010_COMMAND 0x80
#define VCNL4010_PRODUCTID 0x81
#define VCNL4010_PROXRATE 0x82
#define VCNL4010_IRLED 0x83
#define VCNL4010_AMBIENTPARAMETER 0x84
#define VCNL4010_AMBIENTRESULT1 0x85
#define VCNL4010_AMBIENTRESULT0 0x86
#define VCNL4010_PROXIMITYRESULT1 0x87
#define VCNL4010_PROXIMITYRESULT0 0x88
#define VCNL4010_INTCONTROL 0x89
#define VCNL4010_LOWTHRESHOLD1 0x8A
#define VCNL4010_LOWTHRESHOLD0 0x8B
#define VCNL4010_HITHRESHOLD1 0x8C
#define VCNL4010_HITHRESHOLD0 0x8D
#define VCNL4010_INTERRUPTSTATUS 0x8E
#define VCNL4010_MODTIMING 0x8F
#define VCNL4010_AMBIENTIRLEVEL 0x90 // NOT INTENDED FOR USE BY CUSTOMER

// VCNL4010 Product ID Register Bit Definitions
typedef struct {
  uint8_t product_id: 4;
  uint8_t revision_id: 4;
} VCNL4010ProductId;

// VCNL4010 Command Register Bit Definitions (reverse order)
typedef struct
{
  uint8_t self_timed_en : 1;
  uint8_t prox_en : 1;
  uint8_t a_light_en : 1;
  uint8_t prox_od : 1;
  uint8_t a_light_od : 1;
  uint8_t prox_data_rdy : 1;
  uint8_t a_light_dr : 1;
  uint8_t config_lock : 1;
} VCNL4010CommandRegister;

// VCNL4010 Interrupt Control Register Bit Definitions
typedef struct
{
  uint8_t count_exceed : 3;
  uint8_t not_available : 1;
  uint8_t int_prox_read_en : 1;
  uint8_t int_als_ready_en : 1;
  uint8_t int_thresh_en : 1;
  uint8_t int_thresh_sel : 1;
} VCNL4010InterruptControlRegister;

typedef struct
{
  uint8_t ir_led_current_value: 6;
  uint8_t fuse_prog_id: 2;
} VCNL4010IrCurrentRegister;


esp_err_t vcnl4010_init(void);

void vcnl4010_readProximityResult(uint16_t *proximityResult);

void vcnl4010_readAmbientLight(uint16_t *ambientResult);

void vcnl4010_readInterruptStatus(uint8_t *interruptStatus);

void vcnl4010_writeInterruptStatus(uint8_t interruptStatus);

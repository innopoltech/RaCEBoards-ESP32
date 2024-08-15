#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "m_i2c_thread.h"

#pragma once

#define LSM6DSL_DEFAULT_ADDRESS  0x6A
#define LSM6DSL_CHIP_ID          0x6A

//Helpers
#define LSM6DSL_RATE_SHUTDOWN   0
#define LSM6DSL_RATE_12_5_HZ    1
#define LSM6DSL_RATE_26_HZ      2
#define LSM6DSL_RATE_52_HZ      3
#define LSM6DSL_RATE_104_HZ     4
#define LSM6DSL_RATE_208_HZ     5
#define LSM6DSL_RATE_416_HZ     6
#define LSM6DSL_RATE_833_HZ     7
#define LSM6DSL_RATE_1_66K_HZ   8
#define LSM6DSL_RATE_3_33K_HZ   9
#define LSM6DSL_RATE_6_66K_HZ   10
#define LSM6DSL_RATE_1_6_HZ     11

#define LSM6DSL_RANGE_125_DPS     0
#define LSM6DSL_RANGE_250_DPS     1
#define LSM6DSL_RANGE_500_DPS     2
#define LSM6DSL_RANGE_1000_DPS    3
#define LSM6DSL_RANGE_2000_DPS    4

#define LSM6DSL_RANGE_2G          0
#define LSM6DSL_RANGE_16G         1
#define LSM6DSL_RANGE_4G          2
#define LSM6DSL_RANGE_8G          3


typedef struct {
    uint8_t rate;
    uint8_t acc_range;
    uint8_t gyro_range;

    uint8_t   addr;     //I2C addr
    uint8_t   id;       //Chip ID
} lsm6dsl_t;

#define LSM6DSL_ENABLE_INFO         true
#define LSM6DSL_ENABLE_ERROR        true

esp_err_t lsm6dsl_init(lsm6dsl_t *dev);

esp_err_t lsm6dsl_acceleration(lsm6dsl_t *dev, float* data); //m/s^2
esp_err_t lsm6dsl_gyro(lsm6dsl_t *dev, float* data);         //degree per second
esp_err_t lsm6dsl_temperature(lsm6dsl_t *dev, float* data);

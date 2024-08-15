#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <esp_err.h>
#include "driver/i2c.h"

#pragma once

#define I2C_ENABLE_INFO         true
#define I2C_ENABLE_ERROR        true

#define I2C_MASTER_SDA          GPIO_NUM_3
#define I2C_MASTER_SCL          GPIO_NUM_2

esp_err_t i2c_t_create();
esp_err_t i2c_t_close();

BaseType_t i2c_t_take_sem(uint32_t ms_to_wait);
esp_err_t i2c_t_give_sem();
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "m_i2c_thread.h"

#pragma once

//Based on https://github.com/UncleRus/esp-idf-lib/tree/master

#define QMC5883L_ENABLE_INFO         true
#define QMC5883L_ENABLE_ERROR        true

#define QMC5883L_I2C_ADDR_DEF 0x0D
#define QMC5883L_I2C_ID_DEF   0xFF

typedef enum {
    QMC5883L_DR_10 = 0, //10Hz
    QMC5883L_DR_50,     //50Hz
    QMC5883L_DR_100,    //100Hz
    QMC5883L_DR_200,    //200Hz
} qmc5883l_odr_t;

typedef enum {
    QMC5883L_OSR_64 = 0, //64 samples
    QMC5883L_OSR_128,    //128 samples
    QMC5883L_OSR_256,    //256 samples
    QMC5883L_OSR_512,    //512 samples
} qmc5883l_osr_t;

typedef enum {
    QMC5883L_RNG_2 = 0,//-2G..+2G
    QMC5883L_RNG_8     //-8G..+8G
} qmc5883l_range_t;

typedef enum {
    QMC5883L_MODE_STANDBY = 0, //Low power mode, no measurements
    QMC5883L_MODE_CONTINUOUS   //Continuous measurements
} qmc5883l_mode_t;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} qmc5883l_raw_data_t;


typedef struct {
    uint8_t   addr;     //I2C addr
    uint8_t   id;       //Chip ID
    qmc5883l_range_t range;
} qmc5883l_t;


esp_err_t qmc5883l_init(qmc5883l_t *dev, qmc5883l_odr_t odr, qmc5883l_osr_t osr, qmc5883l_range_t rng);

esp_err_t qmc5883l_get_data(qmc5883l_t *dev, float *data);    //Milligauss
esp_err_t qmc5883l_get_raw_data(qmc5883l_t *dev, qmc5883l_raw_data_t *raw);
esp_err_t qmc5883l_raw_to_mg(qmc5883l_t *dev, qmc5883l_raw_data_t *raw, float *data);

esp_err_t qmc5883l_get_temp(qmc5883l_t *dev, float *temp);

esp_err_t qmc5883l_reset(qmc5883l_t *dev);
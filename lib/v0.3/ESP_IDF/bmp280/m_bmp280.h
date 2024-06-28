#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "m_i2c_thread.h"

#pragma once

//Based on https://github.com/UncleRus/esp-idf-lib/tree/master

#define BMP280_ENABLE_INFO         true
#define BMP280_ENABLE_ERROR        true


#define BMP280_I2C_ADDRESS_0  0x76
#define BMP280_I2C_ADDRESS_1  0x77

#define BMP280_CHIP_ID  0x58
#define BME280_CHIP_ID  0x60


typedef enum {
    BMP280_MODE_SLEEP = 0,  //Sleep mode
    BMP280_MODE_FORCED = 1, //Measurement is initiated by user
    BMP280_MODE_NORMAL = 3  //Continues measurement
} BMP280_Mode;

typedef enum {
    BMP280_FILTER_OFF = 0,
    BMP280_FILTER_2 = 1,
    BMP280_FILTER_4 = 2,
    BMP280_FILTER_8 = 3,
    BMP280_FILTER_16 = 4
} BMP280_Filter;


typedef enum {
    BMP280_SKIPPED = 0,          //no measurement
    BMP280_ULTRA_LOW_POWER = 1,  //oversampling x1
    BMP280_LOW_POWER = 2,        //oversampling x2
    BMP280_STANDARD = 3,         //oversampling x4
    BMP280_HIGH_RES = 4,         //oversampling x8
    BMP280_ULTRA_HIGH_RES = 5    //oversampling x16
} BMP280_Oversampling;

typedef enum {
    BMP280_STANDBY_05 = 0,      //stand by time 0.5ms
    BMP280_STANDBY_62 = 1,      //stand by time 62.5ms
    BMP280_STANDBY_125 = 2,     //stand by time 125ms
    BMP280_STANDBY_250 = 3,     //stand by time 250ms
    BMP280_STANDBY_500 = 4,     //stand by time 500ms
    BMP280_STANDBY_1000 = 5,    //stand by time 1s
    BMP280_STANDBY_2000 = 6,    //stand by time 2s BMP280, 10ms BME280
    BMP280_STANDBY_4000 = 7,    //stand by time 4s BMP280, 20ms BME280
} BMP280_StandbyTime;


typedef struct {
    BMP280_Mode mode;
    BMP280_Filter filter;
    BMP280_Oversampling oversampling_pressure;
    BMP280_Oversampling oversampling_temperature;
    BMP280_Oversampling oversampling_humidity;
    BMP280_StandbyTime standby;
} bmp280_params_t;


typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    /* Humidity compensation for BME280 */
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;

    uint8_t   addr;     //I2C addr
    uint8_t   id;       //Chip ID
} bmp280_t;


esp_err_t bmp280_init_default_params(bmp280_params_t *params);
esp_err_t bmp280_init(bmp280_t *dev, bmp280_params_t *params);

esp_err_t bmp280_force_measurement(bmp280_t *dev);
esp_err_t bmp280_is_measuring(bmp280_t *dev, bool *busy);
esp_err_t bmp280_read_float(bmp280_t *dev, float *temperature,
                            float *pressure);

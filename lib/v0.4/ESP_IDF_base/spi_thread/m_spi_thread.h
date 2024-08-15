#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include <esp_err.h>

#pragma once

#define SPI_ENABLE_INFO         true
#define SPI_ENABLE_ERROR        true

#define PIN_NUM_MISO  GPIO_NUM_13
#define PIN_NUM_MOSI  GPIO_NUM_11
#define PIN_NUM_CLK   GPIO_NUM_12

esp_err_t spi_t_create();
esp_err_t spi_t_close();

sdmmc_host_t* spi_t_get_host();

BaseType_t spi_t_take_sem(uint32_t ms_to_wait);
esp_err_t spi_t_give_sem();
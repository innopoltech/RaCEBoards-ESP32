
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"

#include <sys/unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma once

#define FLASH_FAT_PARTITION_NAME "flash"
#define FLASH_FAT_MOUNT_POINT "/flash"
#define FLASH_FAT_LOG         true

#define FLASH_FAT_SPEED_TEST_CYCLE 1
#define FLASH_FAT_SPEED_TEST_SIZE  (10.0f*1024.0f) //byte

#define FLASH_FAT_SCK_FREQ_MHZ  20
#define FLASH_FAT_SPI_HOST      SPI3_HOST

//Required pins for operation
typedef struct
{
    gpio_num_t nCS;
    gpio_num_t SCK;
    gpio_num_t MOSI;
    gpio_num_t MISO;
}flash_fat_pins_t;

esp_err_t flash_fat_mount(flash_fat_pins_t* flash_fat_pins_, BaseType_t core);
esp_err_t flash_fat_unmount();

void flash_fat_test_speed();

void flash_fat_clear_log(uint8_t is_once);
void flash_fat_WriteOnceNextLine();
void flash_fat_WriteOnce(char* msg);
void flash_fat_WriteContinuous(char* msg, uint8_t enable, uint8_t flush);
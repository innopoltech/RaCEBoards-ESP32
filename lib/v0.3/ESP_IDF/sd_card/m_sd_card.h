
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#pragma once

#define MOUNT_POINT "/sdcard"

#define PIN_NUM_CS    GPIO_NUM_15

#define SDCARD_ENABLE_INFO         true
#define SDCARD_ENABLE_ERROR        true

esp_err_t sdcard_mount(sdmmc_host_t* host_);
void sdcard_unmount();

void sdcard_test_speed();

void sdcard_clear_log(uint8_t is_once);

void WriteOnceNextLine();
void WriteOnce(char* msg);
void WriteContinuous(char* msg, uint8_t enable, uint8_t flush);

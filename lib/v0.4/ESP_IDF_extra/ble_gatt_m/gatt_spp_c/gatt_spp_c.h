#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "nvs_flash.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#pragma once

#define M_SPP_C_BLOCK_LEN 240
#define BLE_GATT_SPP_C_LOG_DEGUG false
#define BLE_GATT_SPP_C_SCAN_DEGUG false

typedef struct {
    uint8_t data[M_SPP_C_BLOCK_LEN];
    int length;

    bool use_noreply;
    uint8_t indx;
} m_spp_c_data_block_t;

esp_err_t gatt_spp_c_start(BaseType_t core);
esp_err_t gatt_spp_c_stop(void);

bool spp_set_bda_in_table(uint8_t* bda, uint8_t indx);

esp_err_t  gatt_spp_c_copy_rx(m_spp_c_data_block_t* rx_block);
esp_err_t  gatt_spp_c_request_send(m_spp_c_data_block_t* tx_block);



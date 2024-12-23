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
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma once

//Detail information of the configurating GATT
//https://github.com/espressif/esp-idf/blob/release/v5.1/examples/bluetooth/bluedroid/ble/gatt_server_service_table/tutorial/Gatt_Server_Service_Table_Example_Walkthrough.md
//https://github.com/espressif/esp-idf/blob/release/v5.1/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md

/*
Emulating SPP on Bluethooth LE
*/

#define M_SPP_BLOCK_LEN 240
#define BLE_GATT_SPP_LOG_DEGUG false

typedef struct {
    uint8_t data[M_SPP_BLOCK_LEN];
    int length;
    bool use_notify;
} m_spp_data_block_t;

esp_err_t gatt_spp_start(BaseType_t core);
esp_err_t gatt_spp_stop(void);

esp_err_t  gatt_spp_copy_rx(m_spp_data_block_t* rx_block);
esp_err_t  gatt_spp_request_send(m_spp_data_block_t* tx_block);



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "nvs_flash.h"

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

esp_err_t gatt_echo_start(void);
esp_err_t gatt_echo_stop(void);



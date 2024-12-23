#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma once

#define BLE_LOW_MTU_SIZE 250
#define BLE_LOW_MTU_SIZE_DEFAULT 20

typedef void (*gap_event_handler_f)(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
typedef void (*gatts_event_handler_f) (esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
typedef void (*gattc_event_handler_f) (esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

esp_err_t ble_start(void);
esp_err_t ble_stop(void);

esp_err_t  ble_gatt_register_app(uint32_t app_id, gatts_event_handler_f gatts_event_handler, gap_event_handler_f gap_event_handler);
esp_err_t  ble_gattc_register_app(uint32_t app_id, gattc_event_handler_f gattc_event_handler, gap_event_handler_f gap_event_handler);
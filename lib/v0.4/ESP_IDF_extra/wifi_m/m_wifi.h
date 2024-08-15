
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"

#pragma once

/*
    ! Only STA, not AP
*/ 


typedef enum {
    WIFI_NOT_RUNNING = 0,        //Task not started
    WIFI_ILDE,                   //Task started, not connected
    WIFI_PROCESS,                //Task started, connection is being established
    WIFI_ACTIVE                  //Task started, connection is established
} m_wifi_state;

typedef struct 
{
    char ssid[128];
    char password[128];
    uint8_t request;
} m_wifi_request;


esp_err_t  wifi_start_wifi_task(void);
esp_err_t  wifi_stop_wifi_task(void);

esp_err_t  wifi_connect_to_ap(char* ssid, char* password);
esp_err_t  wifi_dissconnect_from_ap(void);


m_wifi_state wifi_get_status(void);
esp_ip4_addr_t wifi_get_ip(void);

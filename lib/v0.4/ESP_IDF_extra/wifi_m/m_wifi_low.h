
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <string.h>

#pragma once

typedef enum
{
    disconnect_nodata = 0,
    connected,
    lost,
    busy
}m_wifi_low_state;

bool m_wifi_low_try_connect(char* ssid, char* password, bool is_ap);
bool m_wifi_low_try_dissconnect(void);

m_wifi_low_state m_wifi_get_state(void);
esp_ip4_addr_t m_wifi_get_ip(void);
void m_wifi_update_reconnect(void);
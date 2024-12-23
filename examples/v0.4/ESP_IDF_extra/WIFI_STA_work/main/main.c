#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi_m/m_wifi.h"

static const char *TAG = "wifi_sta";



void app_main(void)
{
    while(1)
    {
        wifi_start_wifi_task(PRO_CPU_NUM);
        wifi_connect_to_ap("", "");

        while(wifi_get_status() != WIFI_ACTIVE){vTaskDelay(100 / portTICK_PERIOD_MS);}
        esp_ip4_addr_t ip_info = wifi_get_ip();
        ESP_LOGI(TAG, "MY ip:" IPSTR, IP2STR(&ip_info));

        vTaskDelay(10000 / portTICK_PERIOD_MS);

        wifi_dissconnect_from_ap();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        wifi_stop_wifi_task();

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

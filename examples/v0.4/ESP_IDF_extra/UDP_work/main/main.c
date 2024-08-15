#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "wifi_m/m_wifi.h"
#include "udp_m/m_udp.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char *TAG = "udp";

#define PORT 5000

void app_main(void)
{

        //Connect to wifi
        ESP_LOGI(TAG, "Connect to wifi...");
        wifi_start_wifi_task();

        wifi_connect_to_ap("OOO Roga and hooves", "NetAnime34");
        while(wifi_get_status() != WIFI_ACTIVE){vTaskDelay(100 / portTICK_PERIOD_MS);}

        esp_ip4_addr_t ip_info = wifi_get_ip();
        ESP_LOGI(TAG, "Connected to WIFI AP! IP: " IPSTR, IP2STR(&ip_info));
    while(1)
    {
        //Create UDP server
        while(udp_get_status() == UDP_NOT_RUNNING)
        {
            udp_start_udp_task(PORT);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //RX buff
        uint8_t buff[512];
        uint32_t len=0;
        uint32_t max_len = sizeof(buff)/sizeof(uint8_t);

        while(1)
        {   
            if(udp_copy_rx(buff, &len, max_len) == ESP_OK)
            {
                ESP_LOGI(TAG, "Text recievied: %s", buff);
                if(strncmp((char*)buff, "exit", 4) == 0)
                    break;
            }        
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //Close udp
        while(udp_get_status() == UDP_ACTIVE)
        {
            udp_stop_udp_task();
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

    }
        //Disconnect from wifi
        wifi_dissconnect_from_ap();
        while(wifi_get_status() != WIFI_ILDE){vTaskDelay(100 / portTICK_PERIOD_MS);}
        wifi_stop_wifi_task();


}

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "m_wifi_low.h"


static const char *TAG = "WIFI_lib_low";
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

esp_netif_t *wifiInterface;

static EventGroupHandle_t s_wifi_event_group;

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

static m_wifi_low_state state = disconnect_nodata;
static int s_retry_num = 0;



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    //https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi.html
    
    //Start -> connect
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } 
    
    //Set flag if already has been connected, else try
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {   
        if(state == busy)
        {
            if (s_retry_num < 10) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "Retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG,"Connect to the AP fail");
        }
        else
        {
            state = lost;
        }
    }
    
    //Get ip
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_retry_num = 0;
    }

    else
    {
        ESP_LOGI(TAG, "Unexpected %lu:", event_id);
    }
}

static bool wifi_init_sta(char* ssid, char* password)
{
    state = busy;

    static bool netif_call = false;
    static bool register_callbacks = false;
    static bool wifi_sta = false;


    //Start routine
    s_wifi_event_group = xEventGroupCreate();
    
    //Call netif only once!
    if(netif_call == false)
        if(esp_netif_init() != ESP_OK)
            goto error;
    netif_call = true;

    //Create loop, if not exist
    esp_err_t ret = esp_event_loop_create_default();  
    if( ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        goto error;

    //Create wifi_sta
    if(wifi_sta == false)
        wifiInterface = esp_netif_create_default_wifi_sta();
    wifi_sta = true;

    //Create default wifi config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&cfg) != ESP_OK)
        goto error;

    //Register callbacks
    if(register_callbacks == false)
    {
        if( esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &event_handler,
                                            NULL,
                                            &instance_any_id) != ESP_OK) goto error;
        if( esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &event_handler,
                                            NULL,
                                            &instance_got_ip) != ESP_OK) goto error;
        register_callbacks = true;
    }

    //Setup wifi config
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "test",
            .password = "123456789",
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    strcpy((char*)wifi_config.sta.ssid,    ssid);
    strcpy((char*)wifi_config.sta.password,password);

    //Start wifi
    if(esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        goto error;
    if(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK)
        goto error;
    if(esp_wifi_start() != ESP_OK)
        goto error;

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - init ok");

    //Create bit-flag
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            30000 / portTICK_PERIOD_MS);

    //Wait first bit-flag
    bool connect = false;
    if (bits & WIFI_CONNECTED_BIT) 
    {
        ESP_LOGI(TAG, "Successful connected to AP - SSID:%s password:%s", ssid, password);
        connect = true;
    }else
    {
        ESP_LOGI(TAG, "Failed to connected to AP - SSID:%s password:%s", ssid, password);
    }

    //Ok
    if(connect == true)
    {
        state = connected;
        return true;
    }

error:
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - init failed");
    state = disconnect_nodata;
    return false;    
}


bool m_wifi_low_try_connect(char* ssid, char* password)
{
    if(state == connected)
    {
        ESP_LOGW(TAG, "Wifi is already connected!");
        return false;
    }

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {   
        //Erase
        if(nvs_flash_erase() != ESP_OK)
        {
            ESP_LOGE(TAG, "Not able connect to AP - NVS error");
            return false;
        }

        //Try init
        if(nvs_flash_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Not able connect to AP - NVS error");
            return false;
        }
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - start routine");
    m_wifi_low_try_dissconnect();
    bool ret_b = wifi_init_sta(ssid, password);

    if (ret_b == false)
        ESP_LOGE(TAG, "ESP_WIFI_MODE_STA - routine error");
    else
        ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - routine successful");

    return ret_b;
}


bool  m_wifi_low_try_dissconnect(void)
{
    //Stop sequence
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    //state
    ESP_LOGI(TAG, "Wifi disconnected!");
    state = disconnect_nodata;
    return true;
}

m_wifi_low_state m_wifi_get_state(void)
{
    return state;
}

esp_ip4_addr_t m_wifi_get_ip(void)
{
    if(esp_netif_is_netif_up(wifiInterface))
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(wifiInterface, &ip_info);
        return ip_info.ip;
    }
    else
    {
        esp_ip4_addr_t ip_zero;
        ip_zero.addr = 0;
        return ip_zero;
    }
}

void m_wifi_update_reconnect(void)
{
    s_retry_num = 0;
    if(state == lost)
    {
        state = busy;
        esp_wifi_connect();
    }    
}
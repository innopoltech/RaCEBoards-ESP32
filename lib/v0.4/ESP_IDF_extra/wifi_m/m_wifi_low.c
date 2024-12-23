#include "m_wifi_low.h"

static const char *TAG = "WIFI_lib_low";
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

esp_netif_t *wifiInterface;
esp_netif_t *wifiInterface_ap;

static EventGroupHandle_t s_wifi_event_group;

static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;
bool mode_softap = false;

static m_wifi_low_state state = disconnect_nodata;
static int s_retry_num = 0;

static bool netif_call = false;
static bool register_callbacks = false;
static bool wifi_sta = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    //https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/wifi.html
    
    /*** SOFTAP ***/
    if (event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "SOFTAP station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } 
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "SOFTAP station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }

    /*** STA ***/
    //Start -> connect
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } 
    
    //Set flag if already has been connected, else try reconnect
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
                state = lost;
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
        ESP_LOGI(TAG, "Unexpected: %lu", event_id);
    }
}

static bool wifi_init_sta(char* ssid, char* password)
{
    state = busy;

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
    esp_wifi_set_ps(WIFI_PS_NONE);          // !!!Increase power consuption!!! and !!!reduce latensy!!!
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

static bool wifi_init_softap(char* ssid, char* password)
{
    state = busy;

    //Call netif only once!
    if(netif_call == false)
        if(esp_netif_init() != ESP_OK)
            goto error;
    netif_call = true;

    //Create loop, if not exist
    esp_err_t ret = esp_event_loop_create_default();  
    if( ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        goto error;

    wifiInterface_ap = esp_netif_create_default_wifi_ap();
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

    //Create template
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "test",
            .ssid_len = 4,
            .channel = 1,
            .password = "123456789",
            .max_connection = 8,
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };

    //Copy new
    strcpy((char*)wifi_config.ap.ssid,    ssid);
    wifi_config.ap.ssid_len = strlen(ssid);
    if (password == NULL) 
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    else
        strcpy((char*)wifi_config.ap.password,password);

    //Start wifi
    if(esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK)
        goto error;
    esp_wifi_set_ps(WIFI_PS_NONE);          // !!!Increase power consuption!!! and !!!reduce latensy!!!
    if(esp_wifi_set_config(WIFI_IF_AP, &wifi_config) != ESP_OK)
        goto error;
    if(esp_wifi_start() != ESP_OK)
        goto error;

    if (password == NULL) 
        ESP_LOGI(TAG, "Init_softap finished. SSID:%s (no password!) channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.channel);
    else
        ESP_LOGI(TAG, "Init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);

    state = connected;
    return true;

error:
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP - init failed");
    state = disconnect_nodata;
    return false;    
}

bool m_wifi_low_try_connect(char* ssid, char* password, bool is_ap)
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
            ESP_LOGE(TAG, "Not able connect - NVS error");
            return false;
        }

        //Try init
        if(nvs_flash_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Not able connect - NVS error");
            return false;
        }
    }

    //STA
    if(is_ap == false)  
    {
        mode_softap = false;

        ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - start routine");
        m_wifi_low_try_dissconnect();
        bool ret_b = wifi_init_sta(ssid, password);

        if (ret_b == false)
            ESP_LOGE(TAG, "ESP_WIFI_MODE_STA - routine error");
        else
            ESP_LOGI(TAG, "ESP_WIFI_MODE_STA - routine successful");

        return ret_b;
    }
    //SOFTAP
    else
    {
        mode_softap = true;

        ESP_LOGI(TAG, "ESP_WIFI_MODE_SOFTAP - start routine");
        m_wifi_low_try_dissconnect();
        bool ret_b = wifi_init_softap(ssid, password);

        if (ret_b == false)
            ESP_LOGE(TAG, "ESP_WIFI_MODE_SOFTAP - routine error");
        else
            ESP_LOGI(TAG, "ESP_WIFI_MODE_SOFTAP - routine successful");

        return ret_b;
    }
}


bool  m_wifi_low_try_dissconnect(void)
{
    //Stop sequence
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    vTaskDelay(100 / portTICK_PERIOD_MS);

    //Delete event
    if(s_wifi_event_group != NULL)
        vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

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
    if(mode_softap == false && esp_netif_is_netif_up(wifiInterface))
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(wifiInterface, &ip_info);
        return ip_info.ip;
    }
    else if(mode_softap == true && esp_netif_is_netif_up(wifiInterface_ap))
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(wifiInterface_ap, &ip_info);
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
    if(mode_softap == true)
        return;

    s_retry_num = 0;
    if(state == lost)
    {
        state = busy;
        esp_wifi_connect();
    }    
}
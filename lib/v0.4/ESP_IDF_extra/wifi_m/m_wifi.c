#include "m_wifi.h"
#include "esp_log.h"
#include "m_wifi_low.h"


#include "freertos/queue.h"

#define M_WIFI_CONNECT_REQUEST 0
#define M_WIFI_CLOSE_REQUEST   1

static const char *TAG = "WIFI_lib";

//Loop
static TaskHandle_t wifi_loop_handle = NULL;
static bool wifi_loop_request_close = false;
static void wifi_loop();

//Queue
static QueueHandle_t wifi_task_queue = NULL;

//State
static m_wifi_state wifi_state = WIFI_NOT_RUNNING;


esp_err_t  wifi_start_wifi_task(void)
{   
    //Check running task
    if(wifi_loop_handle != NULL)
    {
        ESP_LOGW(TAG, "WIFI Task is already started!");
        return ESP_FAIL;
    }

    //Create task
    xTaskCreatePinnedToCore(&wifi_loop, "wifi_loop", 1024*4, NULL, 5, &wifi_loop_handle, 0);

    return ESP_OK;
}

esp_err_t  wifi_stop_wifi_task(void)
{
    //Check running task
    if(wifi_loop_handle == NULL)
    {
        ESP_LOGW(TAG, "WIFI Task is already stopped!");
        return ESP_FAIL;
    }

    //Create request to stop task
    if(wifi_loop_request_close != true)
        wifi_loop_request_close = true;

    //Timeout 100*50 = 5000 ms
    for(uint16_t i=0; i<100; i++)
    {
        if(wifi_loop_request_close == false)
            break;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    //Not stopped
    if(wifi_loop_request_close == true)
    {
        ESP_LOGE(TAG, "WIFI Task hasn't been stopped!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t  wifi_connect_to_ap(char* ssid, char* password)
{
    //Check running task
    if(wifi_loop_handle == NULL)
    {
        ESP_LOGE(TAG, "WIFI Task is not running!");
        return ESP_FAIL;
    }

    //Check existing queue
    if(wifi_task_queue == NULL)
    {
        ESP_LOGW(TAG, "WIFI Task queue not exist!");
        return ESP_FAIL;
    }

    //Create request
    m_wifi_request req;
    req.request = M_WIFI_CONNECT_REQUEST;
    strncpy(req.ssid,     ssid,     sizeof(req.ssid)/sizeof(char));
    strncpy(req.password, password, sizeof(req.password)/sizeof(char));

    //Try Send
    if( xQueueSend(wifi_task_queue, &req, (TickType_t)5 ) != pdPASS)
    {
        ESP_LOGE(TAG, "WIFI Task queue is FULL!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WIFI request to connect to AP created!");
    return ESP_OK;
}
esp_err_t  wifi_dissconnect_from_ap(void)
{
    //Check running task
    if(wifi_loop_handle == NULL)
    {
        ESP_LOGE(TAG, "WIFI Task is not running!");
        return ESP_FAIL;
    }

    //Check existing queue
    if(wifi_task_queue == NULL)
    {
        ESP_LOGW(TAG, "WIFI Task queue not exist!");
        return ESP_FAIL;
    }

    //Create request
    m_wifi_request req;
    req.request = M_WIFI_CLOSE_REQUEST;

    //Try Send
    if( xQueueSend(wifi_task_queue, &req, (TickType_t)5 ) != pdPASS)
    {
        ESP_LOGE(TAG, "WIFI Task queue is FULL!");
        return ESP_FAIL;
    }   

    ESP_LOGI(TAG, "WIFI request to dissconnect from AP created!");
    return ESP_OK;
}




static void wifi_loop()
{
    //Create input queue
    wifi_task_queue = xQueueCreate(3, sizeof(m_wifi_request)); 

    //Started
    wifi_loop_request_close = false;
    wifi_state = WIFI_ILDE;
    ESP_LOGI(TAG, "WIFI Task is started!");

    //Request buffer
    m_wifi_request active_request;


    while(1)
    {
        //Check exit
        if(wifi_loop_request_close == true)
            break;

        if( xQueueReceive(wifi_task_queue, &active_request, 0) == pdTRUE)
        {   
            if(active_request.request == M_WIFI_CONNECT_REQUEST)
            {
                wifi_state = WIFI_PROCESS;
                ESP_LOGI(TAG, "<- Try connect to AP ->");
                if( m_wifi_low_try_connect(active_request.ssid, active_request.password) )
                {
                    ESP_LOGI(TAG, "<- Connected ->");
                    wifi_state = WIFI_ACTIVE;
                }
                else
                {
                    ESP_LOGE(TAG, "<- Not Connected ->");
                    wifi_state = WIFI_ILDE;
                }
            }
                
            if(active_request.request == M_WIFI_CLOSE_REQUEST)
            {
                wifi_state = WIFI_PROCESS;
                ESP_LOGI(TAG, "<- Try disconnected from AP ->");
                if( m_wifi_low_try_dissconnect() )
                    ESP_LOGI(TAG, "<- Disconnected ->");
                else
                    ESP_LOGE(TAG, "<- Disconnection failed ->");
                wifi_state = WIFI_ILDE;
            }
        }

        //Check reconnection
        m_wifi_low_state state = m_wifi_get_state();
        if(state == lost)
            m_wifi_update_reconnect();
    
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    //Completion sequence 

    vQueueDelete(wifi_task_queue);
    wifi_task_queue = NULL;

    //Task stop
    ESP_LOGI(TAG, "WIFI Task is stopped!");
    wifi_loop_request_close = false;
    wifi_state = WIFI_NOT_RUNNING;

    //Stopped
    wifi_loop_handle = NULL;
    vTaskDelete(wifi_loop_handle);
}


m_wifi_state wifi_get_status(void)
{
    return wifi_state;
}

esp_ip4_addr_t wifi_get_ip(void)
{
    return m_wifi_get_ip();
}
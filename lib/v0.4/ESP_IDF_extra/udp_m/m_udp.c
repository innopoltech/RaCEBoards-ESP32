#include "m_udp.h"
#include "esp_log.h"

#include "freertos/queue.h"

static const char *TAG = "udp_lib";

//Loop
static TaskHandle_t udp_loop_handle = NULL;
static bool udp_loop_request_close = false;
static void udp_loop();

//Queue
// static QueueHandle_t udp_tx_queue = NULL;
static QueueHandle_t udp_rx_queue = NULL;

//State
static m_udp_state udp_state = UDP_NOT_RUNNING;


esp_err_t  udp_start_udp_task(uint16_t listen_port)
{   
    static uint16_t rx_port;

    //Check running task
    if(udp_loop_handle != NULL)
    {
        ESP_LOGW(TAG, "UDP Task is already started!");
        return ESP_FAIL;
    }

    //Create task
    rx_port = listen_port;
    xTaskCreatePinnedToCore(&udp_loop, "udp_loop", 1024*4, &rx_port, 4, &udp_loop_handle, 0);

    return ESP_OK;
}

esp_err_t  udp_stop_udp_task(void)
{
    //Check running task
    if(udp_loop_handle == NULL)
    {
        ESP_LOGW(TAG, "UDP Task is already stopped!");
        return ESP_FAIL;
    }

    //Create request to stop task
    if(udp_loop_request_close != true)
        udp_loop_request_close = true;

    //Timeout 100*50 = 5000 ms
    for(uint16_t i=0; i<100; i++)
    {
        if(udp_loop_request_close == false)
            break;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

    //Not stopped
    if(udp_loop_request_close == true)
    {
        ESP_LOGE(TAG, "UDP Task hasn't been stopped!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t  udp_request_send(uint32_t ip, uint16_t port, uint8_t* data, uint32_t len)
{
    ESP_LOGE(TAG, "udp_request_send not implement!");
    return ESP_FAIL;
}

esp_err_t  udp_copy_rx(uint8_t* data, uint32_t* len, uint32_t max_len)
{      
    //Reset
    uint8_t buf = 0;
    *len = 0;

    if(max_len<1)
    {
        ESP_LOGE(TAG, "Zero buffer has been provided!");
        return ESP_FAIL;
    }

    while(xQueueReceive(udp_rx_queue, &buf, 0) == pdTRUE )
    {      
        //Save byte
        *data = buf;
        
        //Inc
        data++;
        *len+=1;

        //Check available space
        if(*len+1 >= max_len)
            break;
    }
    
    if(*len == 0)
    {
        //No data
        return ESP_FAIL;
    }

    return ESP_OK;
}


static void udp_loop(void *pvParameters)
{   
    //Create rx queue
    udp_rx_queue = xQueueCreate(1024, sizeof(uint8_t)); 

    //Create var
    char rx_buffer[128];
    char addr_str[128];

    uint16_t port = *((uint16_t*)pvParameters);
    int ip_protocol = 0;
    struct sockaddr_in6 _addr;
    
    struct sockaddr_storage source_addr;
    socklen_t socklen = sizeof(source_addr);

    //Start sequence
    //IP info
    struct sockaddr_in *_addr_ip4 = (struct sockaddr_in *)&_addr;
    _addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    _addr_ip4->sin_family = AF_INET;
    _addr_ip4->sin_port = htons(port);
    ip_protocol = IPPROTO_IP;

    //Create socket
    int sock = socket(AF_INET, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket created");

    //Set timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    //Bind port
    int err = bind(sock, (struct sockaddr *)&_addr, sizeof(_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    //Started
    udp_loop_request_close = false;
    udp_state = UDP_ACTIVE;
    ESP_LOGI(TAG, "UDP Task is started!");

    //loop
    while (1) 
    {
        //Check exit
        if(udp_loop_request_close == true)
            break;

        //Try recievie
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
        
        //Error occurred during receiving
        if(len < 0)
        {   
            //Just timeout 
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
                continue;
            
            //Unexpected error
            ESP_LOGE(TAG, "Recvfrom failed! ERRNO=%d", errno);
            break;
        }

        //Data received
        //Get the sender's ip address as string
        if (source_addr.ss_family == PF_INET)   //ip4
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
        }
        else    //ip6
        {
            ESP_LOGI(TAG, "Received %d bytes from ipv6 (unknown):", len);
        }

        //Null-terminate whatever we received and treat like a string...
        rx_buffer[len] = 0; 
        ESP_LOGI(TAG, "%s", rx_buffer);

        //Add to queue
        uint8_t* ptr = (uint8_t*)rx_buffer;
        while(len>=0) //0 included
        {
            if( xQueueSend(udp_rx_queue, ptr, (TickType_t)5 ) != pdPASS)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            else
            {
                len--;
                ptr++;
            }
            
        }
    }

    //Completion sequence 
error:

    if (udp_state == UDP_ACTIVE) 
    {
        shutdown(sock, 0);
        close(sock);
    }

    vQueueDelete(udp_rx_queue);
    udp_rx_queue = NULL;

    //Task stop
    ESP_LOGI(TAG, "UDP Task is stopped!");
    udp_loop_request_close = false;
    udp_state = UDP_NOT_RUNNING;

    //Stopped
    udp_loop_handle = NULL;
    vTaskDelete(udp_loop_handle);
}


m_udp_state udp_get_status(void)
{
    return udp_state;
}
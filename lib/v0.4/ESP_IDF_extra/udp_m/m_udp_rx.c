#include "m_udp.h"
#include "esp_log.h"

#include "freertos/queue.h"

static const char *TAG = "udp_lib_rx";

/* ######################## RX ######################## */
//Loop
static TaskHandle_t udp_loop_task = NULL;
static bool udp_loop_task_enable = false;
static void udp_loop_rx();

//Queue
static QueueHandle_t udp_rx_queue = NULL;

//State
static m_udp_state udp_state = UDP_NOT_RUNNING;

esp_err_t  udp_start_udp_task_rx(uint16_t listen_port, BaseType_t core)
{   
    static uint16_t rx_port;

    //Check running task
    if(udp_loop_task != NULL)
    {
        ESP_LOGW(TAG, "UDP Task RX is already started!");
        return ESP_FAIL;
    }

    //Create task
    udp_loop_task_enable  = true;
    rx_port = listen_port;
    xTaskCreatePinnedToCore(&udp_loop_rx, "udp_loop_rx", 1024*4, &rx_port, 4, &udp_loop_task, core);

    return ESP_OK;
}

esp_err_t  udp_stop_udp_task_rx(void)
{
    //Check running task
    if(udp_loop_task == NULL)
    {
        ESP_LOGW(TAG, "UDP Task RX is already stopped!");
        return ESP_FAIL;
    }

    //Create request to stop task
    udp_loop_task_enable = false;

    //Timeout 100*50 = 5000 ms
    for(uint16_t i=0; i<100; i++)
    {
        if(udp_loop_task == NULL)
            break;
        vTaskDelay(100/portTICK_PERIOD_MS);
    }

    //Not stopped
    if(udp_loop_task != NULL)
    {
        ESP_LOGE(TAG, "UDP Task RX hasn't been stopped!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t  udp_copy_rx(m_udp_data_block_t* rx_block)
{   
    if(udp_rx_queue == NULL)
    {
        ESP_LOGE(TAG, "UDP Task RX is not started!");
        return ESP_FAIL;
    }

    if(xQueueReceive(udp_rx_queue, (void*)rx_block, 0) != pdTRUE)
    {
        rx_block->length = 0;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void udp_loop_rx(void *pvParameters)
{   
    //Create rx queue
    udp_rx_queue = xQueueCreate(5, sizeof(m_udp_data_block_t)); 

    //Create var
    m_udp_data_block_t rx_block;
    char addr_str[128];

    uint16_t port = *((uint16_t*)pvParameters);
    int ip_protocol = 0;
    struct sockaddr_in6 _addr;
    socklen_t socklen = sizeof(rx_block.src_addr);

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
    timeout.tv_sec = 0;
    timeout.tv_usec = 1e5;  //100 ms
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    //Bind port
    int err = bind(sock, (struct sockaddr *)&_addr, sizeof(_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto error;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    //Started
    udp_state = UDP_ACTIVE;
    ESP_LOGI(TAG, "UDP Task RX is started!");

    //loop
    while (udp_loop_task_enable) 
    {
        //Try recievie
        rx_block.length = recvfrom(sock, rx_block.data, sizeof(rx_block.data) - 2, 0, (struct sockaddr *)&rx_block.src_addr, &socklen);
        
        //Error occurred during receiving
        if(rx_block.length < 0)
        {   
            //Just timeout 
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
                continue;
            
            //Unexpected error
            ESP_LOGE(TAG, "Recvfrom failed! ERRNO=%d", errno);
            taskYIELD();
        }

        //Data received
        //Get the sender's ip address as string
        //Null-terminate whatever we received and treat like a string...
        rx_block.data[rx_block.length] = 0; 
        if(TX_RX_DATA_LOG)
        {
            if (rx_block.src_addr.ss_family == PF_INET)   //ip4
            {
                struct in_addr in_addr_ = ((struct sockaddr_in *)&rx_block.src_addr)->sin_addr;
                in_port_t in_port_ = ((struct sockaddr_in *)&rx_block.src_addr)->sin_port;
                inet_ntoa_r((in_addr_), addr_str, sizeof(addr_str) - 1);
                
                ESP_LOGI(TAG, "Received %d bytes from %s:%d:", rx_block.length , addr_str, ntohs(in_port_));
            }
            else    //ip6
            {
                ESP_LOGI(TAG, "Received %d bytes from ipv6 (unknown):", rx_block.length );
            }

            ESP_LOGI(TAG, "%s", rx_block.data);
        }

        //Add to queue
        while( xQueueSend(udp_rx_queue, (void*)&rx_block, pdMS_TO_TICKS(1000) ) != pdPASS) {;}
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
    ESP_LOGI(TAG, "UDP Task RX is stopped!");
    udp_state = UDP_NOT_RUNNING;

    //Stopped
    udp_loop_task = NULL;
    vTaskDelete(udp_loop_task);
}


m_udp_state udp_get_status_rx(void)
{
    return udp_state;
}
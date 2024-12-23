#include "m_udp.h"
#include "esp_log.h"

#include "freertos/queue.h"

static const char *TAG = "udp_lib_tx";

/* ######################## TX ######################## */
//Loop
static TaskHandle_t udp_loop_task = NULL;
static bool udp_loop_task_enable = false;
static void udp_loop_tx();

//Queue
static QueueHandle_t udp_tx_queue = NULL;

//State
static m_udp_state udp_state = UDP_NOT_RUNNING;


esp_err_t  udp_start_udp_task_tx(m_udp_dst_t udp_dst, BaseType_t core)
{   
    static m_udp_dst_t dst;

    //Check running task
    if(udp_loop_task != NULL)
    {
        ESP_LOGW(TAG, "UDP Task TX is already started!");
        return ESP_FAIL;
    }

    //Create task
    udp_loop_task_enable  = true;
    dst = udp_dst;
    xTaskCreatePinnedToCore(&udp_loop_tx, "udp_loop_tx", 1024*4, &dst, 5, &udp_loop_task, core);

    return ESP_OK;
}

esp_err_t  udp_stop_udp_task_tx(void)
{
    //Check running task
    if(udp_loop_task == NULL)
    {
        ESP_LOGW(TAG, "UDP Task TX is already stopped!");
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
        ESP_LOGE(TAG, "UDP Task TX hasn't been stopped!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t  udp_request_send(m_udp_data_block_t* tx_block)
{
    if(udp_tx_queue == NULL)
    {
        ESP_LOGE(TAG, "UDP Task TX is not started!");
        return ESP_FAIL;
    }

    //Add to queue
    if(xQueueSend(udp_tx_queue, tx_block, pdMS_TO_TICKS(1000) ) != pdPASS)
    {
        ESP_LOGE(TAG, "Queue is full!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void udp_loop_tx(void *pvParameters)
{   
    //Create rx queue
    udp_tx_queue = xQueueCreate(5, sizeof(m_udp_data_block_t)); 

    //Create var
    int ip_protocol = IPPROTO_IP;  
    int addr_family = AF_INET;
    int sock = -1;

    m_udp_dst_t dst = *((m_udp_dst_t*)pvParameters);

    m_udp_data_block_t tx_block;

    struct in_addr in_addr_;
    in_port_t in_port_ ;
    char addr_str[128];
    uint16_t port;

    //Start sequence
    //IP info
    struct sockaddr_in6 dest_addr_address_port;
    struct sockaddr* sockaddr_local;

    //Select resources with destination data
    if(dst.use_sockaddr_storage)    //Use with udp_rx
    {
        sockaddr_local = (struct sockaddr* )&dst.dst_addr;
        if(sockaddr_local->sa_family == AF_INET6)
        {
            ESP_LOGE(TAG, "Unable to create socket with IPv6!");
            goto error;
        }

        in_addr_ = ((struct sockaddr_in *)&dst.dst_addr)->sin_addr;
        in_port_ = ((struct sockaddr_in *)&dst.dst_addr)->sin_port;
        inet_ntoa_r((in_addr_), addr_str, sizeof(addr_str) - 1);
        port = ntohs(in_port_);
    }
    else                            //Use with raw tx
    {
        struct sockaddr_in *_addr_ip4 = (struct sockaddr_in *)&dest_addr_address_port;
        _addr_ip4->sin_addr.s_addr = inet_addr(dst.addres);
        _addr_ip4->sin_family = AF_INET;
        _addr_ip4->sin_port = htons(dst.port);

        sockaddr_local = (struct sockaddr* )&dest_addr_address_port;

        strncpy(addr_str, dst.addres, sizeof(addr_str));
        port = (uint16_t)dst.port;
    }

    //Create socket
    sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto error;
    }

    //Set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1e5;  //100 ms
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    //Started
    udp_state = UDP_ACTIVE;
    ESP_LOGI(TAG, "Socket created! Dst address - %s:%d", addr_str, port);
    ESP_LOGI(TAG, "UDP Task TX is started!");

    //loop
    while (udp_loop_task_enable) 
    {
        //Wait for block in queue
        if(xQueueReceive(udp_tx_queue, (void*)&tx_block, pdMS_TO_TICKS(50) ) == pdPASS)
        {
            if(TX_RX_DATA_LOG)
            {
                if (addr_family == AF_INET)   //ip4
                {
                    ESP_LOGI(TAG, "Send %d bytes to %s:%d:", tx_block.length , addr_str, port);
                }
                else    //ip6
                {
                    (void)addr_family;  //Not implemented
                }
                ESP_LOGI(TAG, "%s", tx_block.data);
            }

            //Try send
            size_t total_sent = 0;
            while (total_sent < tx_block.length) 
            {
                ssize_t bytes_sent = sendto(sock, tx_block.data + total_sent, tx_block.length - total_sent, 0,
                                            sockaddr_local, sizeof(struct sockaddr));
                if (bytes_sent < 0) //Error
                {
                    if(errno == ENOMEM)
                    {   
                        taskYIELD();
                        continue;
                    }
                    else
                    {
                        printf("Send failed: errno %d\n", errno);
                        taskYIELD();
                        break;
                    }
                }
                total_sent += bytes_sent;
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

    vQueueDelete(udp_tx_queue);
    udp_tx_queue = NULL;

    //Task stop
    ESP_LOGI(TAG, "UDP Task TX is stopped!");
    udp_state = UDP_NOT_RUNNING;

    //Stopped
    udp_loop_task = NULL;
    vTaskDelete(udp_loop_task);
}


m_udp_state udp_get_status_tx(void)
{
    return udp_state;
}
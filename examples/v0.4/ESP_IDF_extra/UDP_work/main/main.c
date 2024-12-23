#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "wifi_m/m_wifi.h"
#include "udp_m/m_udp.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <sys/param.h>

#include <string.h>
#include <lwip/netdb.h>

static const char *TAG = "udp";

#define ECHO true

#define RX_PORT 5000

#define TX_ADDR "192.168.0.16"  //Only tx_rx_example
#define TX_PORT 9027            //Used in both example

void slow_echo_example()
{
    //Buffers
    m_udp_data_block_t rx_buff;
    m_udp_data_block_t tx_buff;

    //Create dst address struct
    m_udp_dst_t tx_dst = {0,};

    while(1)
    {
        //Create UDP RX server
        udp_start_udp_task_rx(RX_PORT, PRO_CPU_NUM);

        while(1)
        {   
            //Rec data
            if(udp_copy_rx(&rx_buff) == ESP_OK)
            {
                ESP_LOGI(TAG, "Text recievied: %s", rx_buff.data);
                if(strncmp((char*)rx_buff.data, "exit", 4) == 0)
                    break;

                //Re-Start tx_server if address change
                in_addr_t send_addr = ((struct sockaddr_in *)&tx_dst.dst_addr)->sin_addr.s_addr;
                in_addr_t rec_addr = ((struct sockaddr_in *)&rx_buff.src_addr)->sin_addr.s_addr;
                if( rec_addr != send_addr)
                {
                    //Close udp tx
                    udp_stop_udp_task_tx();

                    //Copy dst data
                    memcpy(&tx_dst.dst_addr, &rx_buff.src_addr, sizeof(rx_buff.src_addr));
                    tx_dst.use_sockaddr_storage = true;

                    //Change UDP port
                    ((struct sockaddr_in *)&tx_dst.dst_addr)->sin_port = (in_port_t)ntohs(TX_PORT);

                    //Create UDP_TX server
                    udp_start_udp_task_tx(tx_dst, PRO_CPU_NUM);
                }

                //Send data
                tx_buff.length = rx_buff.length;
                memcpy(tx_buff.data, rx_buff.data, sizeof(tx_buff.data));
                udp_request_send(&tx_buff);
            }

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        //Close UDP servers
        udp_stop_udp_task_rx();
        udp_stop_udp_task_tx();

        //Clear data
        memset(&tx_dst.dst_addr, 0, sizeof(tx_dst.dst_addr));
    }
}


void tx_rx_example()
{
    //Buffers
    m_udp_data_block_t rx_buff;
    m_udp_data_block_t tx_buff;

    //Fill dst address
    m_udp_dst_t tx_dst = {0,};
    strncpy(tx_dst.addres, TX_ADDR, sizeof(tx_dst.addres));
    tx_dst.port = TX_PORT;
    tx_dst.use_sockaddr_storage = false;

    uint8_t i=0;
    while(1)
    {
        //Create UDP servers
        udp_start_udp_task_rx(RX_PORT, PRO_CPU_NUM);
        udp_start_udp_task_tx(tx_dst, PRO_CPU_NUM);

        while(1)
        {   
            //Rec data
            if(udp_copy_rx(&rx_buff) == ESP_OK)
            {
                ESP_LOGI(TAG, "Text recievied: %s", rx_buff.data);
                if(strncmp((char*)rx_buff.data, "exit", 4) == 0)
                    break;
            }

            //Send data
            tx_buff.length = sprintf((char*)tx_buff.data, "Count - %d", ++i);
            udp_request_send(&tx_buff);

            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        //Close UDP servers
        udp_stop_udp_task_rx();
        udp_stop_udp_task_tx();

    }
}


void app_main(void)
{
    //Connect to wifi
    ESP_LOGI(TAG, "Connect to wifi...");
    wifi_start_wifi_task(PRO_CPU_NUM);

    wifi_connect_to_ap("INNOPOL", "Azbuka1331");
    while(wifi_get_status() != WIFI_ACTIVE){vTaskDelay(100 / portTICK_PERIOD_MS);}

    esp_ip4_addr_t ip_info = wifi_get_ip();
    ESP_LOGI(TAG, "Connected to WIFI AP! IP: " IPSTR, IP2STR(&ip_info));

    //Examples
    if(ECHO)
        slow_echo_example();
    else
        tx_rx_example();


    //Disconnect from wifi
    wifi_dissconnect_from_ap();
    while(wifi_get_status() != WIFI_ILDE){vTaskDelay(100 / portTICK_PERIOD_MS);}
    wifi_stop_wifi_task();


}

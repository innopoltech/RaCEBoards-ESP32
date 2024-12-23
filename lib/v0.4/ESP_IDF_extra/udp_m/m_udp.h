#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <sys/param.h>
#include "esp_event.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <string.h>

#pragma once

#define M_UDP_RX_BLOCK 512
#define TX_RX_DATA_LOG false

typedef struct {
    uint8_t data[M_UDP_RX_BLOCK];
    struct sockaddr_storage src_addr;
    int length;
} m_udp_data_block_t;

typedef struct {
    struct sockaddr_storage dst_addr;
    bool use_sockaddr_storage;
    char addres[32];
    int port;
} m_udp_dst_t;

typedef enum {
    UDP_NOT_RUNNING = 0,        //Task not started
    UDP_ACTIVE                  //Task started
} m_udp_state;


//RX
esp_err_t  udp_start_udp_task_rx(uint16_t listen_port, BaseType_t core);
esp_err_t  udp_stop_udp_task_rx(void);
m_udp_state udp_get_status_rx(void);
esp_err_t  udp_copy_rx(m_udp_data_block_t* rx_block);

//TX
esp_err_t  udp_start_udp_task_tx(m_udp_dst_t udp_dst, BaseType_t core);
esp_err_t  udp_stop_udp_task_tx(void);
m_udp_state udp_get_status_tx(void);
esp_err_t  udp_request_send(m_udp_data_block_t* tx_block);

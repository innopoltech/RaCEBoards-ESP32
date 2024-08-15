
#include <string.h>
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


#pragma once

typedef enum {
    UDP_NOT_RUNNING = 0,        //Task not started
    UDP_ACTIVE                  //Task started
} m_udp_state;


esp_err_t  udp_start_udp_task(uint16_t listen_port);
esp_err_t  udp_stop_udp_task(void);

m_udp_state udp_get_status(void);

esp_err_t  udp_request_send(uint32_t ip, uint16_t port, uint8_t* data, uint32_t len);
esp_err_t  udp_copy_rx(uint8_t* data, uint32_t* len, uint32_t max_len);
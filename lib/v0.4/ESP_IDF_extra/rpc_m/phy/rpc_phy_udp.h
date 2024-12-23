#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "m_udp.h"
#include "rpc_def.h"

#include <string.h>

#pragma once

#define RPC_PHY_UDP_RX_PORT 5000
#define RPC_PHY_UDP_TX_PORT 5100

typedef struct 
{
    m_udp_data_block_t* rx_buff;
    m_udp_data_block_t* tx_buff;

    uint32_t rx_buff_indx;
    uint32_t tx_buff_indx;

    m_udp_dst_t tx_dst;
}rpc_phy_udp_context_t;

//User side
bool  rpc_phy_udp_create(rpc_phy_udp_context_t* rpc_phy_udp_context, bool use_psram);
bool  rpc_phy_udp_delete(rpc_phy_udp_context_t* rpc_phy_udp_context);

bool rpc_phy_udp_start(rpc_phy_udp_context_t* rpc_phy_udp_context);
bool rpc_phy_udp_stop(rpc_phy_udp_context_t* rpc_phy_udp_context);

//RPC side
void rpc_phy_udp_update_rx_buffer(void* context);
void rpc_phy_udp_update_tx_buffer(void* context);

bool rpc_phy_udp_read_byte(void* context, uint8_t* data);
bool rpc_phy_udp_write_bytes(void* context, uint8_t* data, uint32_t len);
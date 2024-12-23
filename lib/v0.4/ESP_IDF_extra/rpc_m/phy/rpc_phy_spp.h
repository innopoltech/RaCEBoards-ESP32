#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gatt_spp.h"
#include "rpc_def.h"

#include <string.h>

#pragma once

typedef struct 
{
    m_spp_data_block_t* rx_buff;
    m_spp_data_block_t* tx_buff;

    uint32_t rx_buff_indx;
    uint32_t tx_buff_indx;

    bool next_tx_use_notify;    //hack
}rpc_phy_spp_context_t;

//User side
bool  rpc_phy_spp_create(rpc_phy_spp_context_t* rpc_phy_spp_context, bool use_psram);
bool  rpc_phy_spp_delete(rpc_phy_spp_context_t* rpc_phy_spp_context);

bool rpc_phy_spp_start(rpc_phy_spp_context_t* rpc_phy_spp_context);
bool rpc_phy_spp_stop(rpc_phy_spp_context_t* rpc_phy_spp_context);

//RPC side
void rpc_phy_spp_update_rx_buffer(void* context);
void rpc_phy_spp_update_tx_buffer(void* context);

bool rpc_phy_spp_read_byte(void* context, uint8_t* data);
bool rpc_phy_spp_write_bytes(void* context, uint8_t* data, uint32_t len);
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gatt_spp_c.h"
#include "rpc_def.h"

#include <string.h>

#pragma once

typedef struct 
{
    m_spp_c_data_block_t* rx_buff;
    m_spp_c_data_block_t* tx_buff;

    uint32_t rx_buff_indx;
    uint32_t tx_buff_indx;

    uint8_t  dev_indx;
}rpc_phy_spp_c_context_t;

//User side
bool  rpc_phy_spp_c_create(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context, bool use_psram);
bool  rpc_phy_spp_c_delete(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context);

bool rpc_phy_spp_c_start(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context);
bool rpc_phy_spp_c_stop(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context);

//RPC side
void rpc_phy_spp_c_update_rx_buffer(void* context);
void rpc_phy_spp_c_update_tx_buffer(void* context);

bool rpc_phy_spp_c_read_byte(void* context, uint8_t* data);
bool rpc_phy_spp_c_write_bytes(void* context, uint8_t* data, uint32_t len);
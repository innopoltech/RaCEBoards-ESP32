#include "rpc_phy_spp.h"

/* ######################## USER ONLY ######################## */
static void reset_context(rpc_phy_spp_context_t* rpc_phy_spp_context)
{
    if(rpc_phy_spp_context->tx_buff != NULL)
        memset(rpc_phy_spp_context->tx_buff, 0, sizeof(m_spp_data_block_t));
    if(rpc_phy_spp_context->rx_buff != NULL)
        memset(rpc_phy_spp_context->rx_buff, 0, sizeof(m_spp_data_block_t));

    rpc_phy_spp_context->tx_buff_indx = 0;
    rpc_phy_spp_context->rx_buff_indx = 0;
}

bool  rpc_phy_spp_create(rpc_phy_spp_context_t* rpc_phy_spp_context, bool use_psram)
{
    rpc_phy_spp_delete(rpc_phy_spp_context);

    //Malloc mem
    uint32_t flag = MALLOC_CAP_8BIT;
    flag |= use_psram ? MALLOC_CAP_SPIRAM : 0;

    rpc_phy_spp_context->tx_buff = (m_spp_data_block_t*)heap_caps_malloc(sizeof(m_spp_data_block_t), flag);
    if(rpc_phy_spp_context->tx_buff == NULL)
        return false;

    rpc_phy_spp_context->rx_buff = (m_spp_data_block_t*)heap_caps_malloc(sizeof(m_spp_data_block_t), flag);
    if(rpc_phy_spp_context->rx_buff == NULL)
        return false;

    reset_context(rpc_phy_spp_context);
    return true;
}

bool  rpc_phy_spp_delete(rpc_phy_spp_context_t* rpc_phy_spp_context)
{
    if( rpc_phy_spp_context->tx_buff != NULL)
        heap_caps_free(rpc_phy_spp_context->tx_buff);
    if( rpc_phy_spp_context->rx_buff != NULL)
        heap_caps_free(rpc_phy_spp_context->rx_buff);

    rpc_phy_spp_context->tx_buff = NULL;
    rpc_phy_spp_context->rx_buff = NULL;
    
    reset_context(rpc_phy_spp_context);
    return true;    
}

bool rpc_phy_spp_start(rpc_phy_spp_context_t* rpc_phy_spp_context)
{
    reset_context(rpc_phy_spp_context);
    
    // //Start ble_gatt_spp
    // gatt_spp_start(PRO_CPU_NUM);

    return true;
}

bool rpc_phy_spp_stop(rpc_phy_spp_context_t* rpc_phy_spp_context)
{
    // //Close ble_gatt_spp
    // gatt_spp_stop();

    reset_context(rpc_phy_spp_context);
    return true;
}

/* ######################## RPC ONLY ######################## */
void rpc_phy_spp_update_rx_buffer(void* context)
{
    rpc_phy_spp_context_t* spp_context = (rpc_phy_spp_context_t*)context;

    //Check remaining data
    if(spp_context->rx_buff->length != 0)
        return;

    //Check new data
    if(gatt_spp_copy_rx(spp_context->rx_buff) != ESP_OK)
        return;

    //Reset index
    spp_context->rx_buff_indx = 0;
}

void rpc_phy_spp_update_tx_buffer(void* context){;}

bool rpc_phy_spp_read_byte(void* context, uint8_t* data)
{   
    //Extract pointers from context
    m_spp_data_block_t* rx_block = ((rpc_phy_spp_context_t*)context)->rx_buff;
    uint32_t* indx = &((rpc_phy_spp_context_t*)context)->rx_buff_indx;

    //Read byte
    if(rx_block->length != 0)
    {
        *data = rx_block->data[*indx];
        *indx = *indx + 1;
        rx_block->length = rx_block->length - 1;

        return true;
    }
    return false;
}

bool rpc_phy_spp_write_bytes(void* context, uint8_t* data, uint32_t len)
{
    //Extract pointers from context
    m_spp_data_block_t* tx_block = ((rpc_phy_spp_context_t*)context)->tx_buff;
    uint32_t* indx = &((rpc_phy_spp_context_t*)context)->tx_buff_indx;
    bool* use_notify = &((rpc_phy_spp_context_t*)context)->next_tx_use_notify;
    *indx = 0; 

    while (len > 0) {
        //Copy data
        tx_block->length = (len > M_SPP_BLOCK_LEN) ? M_SPP_BLOCK_LEN : len;
        memcpy(tx_block->data, &data[*indx], tx_block->length);
        tx_block->use_notify = *use_notify;

        //Send, timeout provided BLE_GATT_SPP_lib
        if(gatt_spp_request_send(tx_block) != ESP_OK)
            return false;

        len -= tx_block->length;
        *indx = *indx + tx_block->length;
    }

    //Reset
    *use_notify = false;

    return true;
}
#include "rpc_phy_spp_c.h"

/* ######################## USER ONLY ######################## */
static void reset_context(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context)
{
    if(rpc_phy_spp_c_context->tx_buff != NULL)
        memset(rpc_phy_spp_c_context->tx_buff, 0, sizeof(m_spp_c_data_block_t));
    if(rpc_phy_spp_c_context->rx_buff != NULL)
        memset(rpc_phy_spp_c_context->rx_buff, 0, sizeof(m_spp_c_data_block_t));

    rpc_phy_spp_c_context->tx_buff_indx = 0;
    rpc_phy_spp_c_context->rx_buff_indx = 0;
}

bool  rpc_phy_spp_c_create(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context, bool use_psram)
{
    rpc_phy_spp_c_delete(rpc_phy_spp_c_context);

    //Malloc mem
    uint32_t flag = MALLOC_CAP_8BIT;
    flag |= use_psram ? MALLOC_CAP_SPIRAM : 0;

    rpc_phy_spp_c_context->tx_buff = (m_spp_c_data_block_t*)heap_caps_malloc(sizeof(m_spp_c_data_block_t), flag);
    if(rpc_phy_spp_c_context->tx_buff == NULL)
        return false;

    rpc_phy_spp_c_context->rx_buff = (m_spp_c_data_block_t*)heap_caps_malloc(sizeof(m_spp_c_data_block_t), flag);
    if(rpc_phy_spp_c_context->rx_buff == NULL)
        return false;

    reset_context(rpc_phy_spp_c_context);
    return true;
}

bool  rpc_phy_spp_c_delete(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context)
{
    if( rpc_phy_spp_c_context->tx_buff != NULL)
        heap_caps_free(rpc_phy_spp_c_context->tx_buff);
    if( rpc_phy_spp_c_context->rx_buff != NULL)
        heap_caps_free(rpc_phy_spp_c_context->rx_buff);

    rpc_phy_spp_c_context->tx_buff = NULL;
    rpc_phy_spp_c_context->rx_buff = NULL;
    
    reset_context(rpc_phy_spp_c_context);
    return true;    
}

bool rpc_phy_spp_c_start(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context)
{
    reset_context(rpc_phy_spp_c_context);
    return true;
}

bool rpc_phy_spp_c_stop(rpc_phy_spp_c_context_t* rpc_phy_spp_c_context)
{
    reset_context(rpc_phy_spp_c_context);
    return true;
}

/* ######################## RPC ONLY ######################## */
void rpc_phy_spp_c_update_rx_buffer(void* context)
{
    rpc_phy_spp_c_context_t* spp_c_context = (rpc_phy_spp_c_context_t*)context;

    //Check remaining data
    if(spp_c_context->rx_buff->length != 0)
        return;

    //Check new data
    spp_c_context->rx_buff->indx = spp_c_context->dev_indx;
    if(gatt_spp_c_copy_rx(spp_c_context->rx_buff) != ESP_OK)
        return;

    //Reset index
    spp_c_context->rx_buff_indx = 0;
}

void rpc_phy_spp_c_update_tx_buffer(void* context){;}

bool rpc_phy_spp_c_read_byte(void* context, uint8_t* data)
{   
    //Extract pointers from context
    rpc_phy_spp_c_context_t* spp_c_context = (rpc_phy_spp_c_context_t*)context;
    m_spp_c_data_block_t* rx_block = spp_c_context->rx_buff;
    uint32_t* indx = &spp_c_context->rx_buff_indx;

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

bool rpc_phy_spp_c_write_bytes(void* context, uint8_t* data, uint32_t len)
{
    //Extract pointers from context
    rpc_phy_spp_c_context_t* spp_c_context = (rpc_phy_spp_c_context_t*)context;
    m_spp_c_data_block_t* tx_block = spp_c_context->tx_buff;
    uint32_t* indx    = &spp_c_context->tx_buff_indx;
    uint8_t* dev_indx = &spp_c_context->dev_indx;
    *indx = 0; 

    while (len > 0) 
    {
        //Copy data
        tx_block->indx = *dev_indx;
        tx_block->length = (len > M_SPP_C_BLOCK_LEN) ? M_SPP_C_BLOCK_LEN : len;
        memcpy(tx_block->data, &data[*indx], tx_block->length);

        //Send, timeout provided BLE_GATT_SPP_lib
        if(gatt_spp_c_request_send(tx_block) != ESP_OK)
            return false;

        len -= tx_block->length;
        *indx = *indx + tx_block->length;
    }

    return true;
}
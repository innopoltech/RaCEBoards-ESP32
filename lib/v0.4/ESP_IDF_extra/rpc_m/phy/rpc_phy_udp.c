#include "rpc_phy_udp.h"

/* ######################## USER ONLY ######################## */
static void reset_context(rpc_phy_udp_context_t* rpc_phy_udp_context)
{
    if(rpc_phy_udp_context->tx_buff != NULL)
        memset(rpc_phy_udp_context->tx_buff, 0, sizeof(m_udp_data_block_t));
    if(rpc_phy_udp_context->rx_buff != NULL)
        memset(rpc_phy_udp_context->rx_buff, 0, sizeof(m_udp_data_block_t));

    rpc_phy_udp_context->tx_buff_indx = 0;
    rpc_phy_udp_context->rx_buff_indx = 0;

    memset((void*)&rpc_phy_udp_context->tx_dst, 0, sizeof(rpc_phy_udp_context->tx_dst));
}

bool  rpc_phy_udp_create(rpc_phy_udp_context_t* rpc_phy_udp_context, bool use_psram)
{
    rpc_phy_udp_delete(rpc_phy_udp_context);

    //Malloc mem
    uint32_t flag = MALLOC_CAP_8BIT;
    flag |= use_psram ? MALLOC_CAP_SPIRAM : 0;

    rpc_phy_udp_context->tx_buff = (m_udp_data_block_t*)heap_caps_malloc(sizeof(m_udp_data_block_t), flag);
    if(rpc_phy_udp_context->tx_buff == NULL)
        return false;

    rpc_phy_udp_context->rx_buff = (m_udp_data_block_t*)heap_caps_malloc(sizeof(m_udp_data_block_t), flag);
    if(rpc_phy_udp_context->rx_buff == NULL)
        return false;

    reset_context(rpc_phy_udp_context);
    return true;
}

bool  rpc_phy_udp_delete(rpc_phy_udp_context_t* rpc_phy_udp_context)
{
    if( rpc_phy_udp_context->tx_buff != NULL)
        heap_caps_free(rpc_phy_udp_context->tx_buff);
    if( rpc_phy_udp_context->rx_buff != NULL)
        heap_caps_free(rpc_phy_udp_context->rx_buff);

    rpc_phy_udp_context->tx_buff = NULL;
    rpc_phy_udp_context->rx_buff = NULL;
    
    reset_context(rpc_phy_udp_context);
    return true;    
}

bool rpc_phy_udp_start(rpc_phy_udp_context_t* rpc_phy_udp_context)
{
    reset_context(rpc_phy_udp_context);

    //Create UDP RX server
    udp_start_udp_task_rx(RPC_PHY_UDP_RX_PORT, PRO_CPU_NUM);

    return true;
}

bool rpc_phy_udp_stop(rpc_phy_udp_context_t* rpc_phy_udp_context)
{
    //Close UDP servers
    udp_stop_udp_task_rx();
    udp_stop_udp_task_tx();

    reset_context(rpc_phy_udp_context);
    return true;
}

/* ######################## RPC ONLY ######################## */
void rpc_phy_udp_update_rx_buffer(void* context)
{
    rpc_phy_udp_context_t* udp_context = (rpc_phy_udp_context_t*)context;

    //Check remaining data
    if(udp_context->rx_buff->length != 0)
        return;

    //Check new data
    if(udp_copy_rx(udp_context->rx_buff) != ESP_OK)
        return;

    //Reset index
    udp_context->rx_buff_indx = 0;

    //Re-Start tx_server if address change
    struct sockaddr_in * dst_addr = (struct sockaddr_in *)&udp_context->tx_dst.dst_addr;
    struct sockaddr_in * src_addr = (struct sockaddr_in *)&udp_context->rx_buff->src_addr;
    in_addr_t send_addr = dst_addr->sin_addr.s_addr;
    in_addr_t rec_addr = src_addr->sin_addr.s_addr;

    if( rec_addr != send_addr)
    {
        //Close udp tx
        udp_stop_udp_task_tx();

        //Copy dst data
        memcpy(dst_addr, src_addr, sizeof(udp_context->rx_buff->src_addr));
        udp_context->tx_dst.use_sockaddr_storage = true;

        //Change UDP port
        dst_addr->sin_port = (in_port_t)ntohs(RPC_PHY_UDP_TX_PORT);

        //Create UDP_TX server
        udp_start_udp_task_tx(udp_context->tx_dst, PRO_CPU_NUM);
    }
}

void rpc_phy_udp_update_tx_buffer(void* context){;}

bool rpc_phy_udp_read_byte(void* context, uint8_t* data)
{   
    //Extract pointers from context
    m_udp_data_block_t* rx_block = ((rpc_phy_udp_context_t*)context)->rx_buff;
    uint32_t* indx = &((rpc_phy_udp_context_t*)context)->rx_buff_indx;

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

bool rpc_phy_udp_write_bytes(void* context, uint8_t* data, uint32_t len)
{
    //Check startet task
    if(udp_get_status_tx() != UDP_ACTIVE)
        return false;

    //Extract pointers from context
    m_udp_data_block_t* tx_block = ((rpc_phy_udp_context_t*)context)->tx_buff;
    uint32_t* indx = &((rpc_phy_udp_context_t*)context)->tx_buff_indx;
    *indx = 0; 

    while (len > 0) {
        //Copy data
        tx_block->length = (len > M_UDP_RX_BLOCK) ? M_UDP_RX_BLOCK : len;
        memcpy(tx_block->data, &data[*indx], tx_block->length);

        //Send, timeout provided m_udp_lib
        if(udp_request_send(tx_block) != ESP_OK)
            return false;

        len -= tx_block->length;
        *indx = *indx + tx_block->length;
    }

    return true;
}
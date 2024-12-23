#include "rpc_data_link.h"

static uint8_t calculate_crc8(uint8_t* data, uint16_t len)
{
    uint8_t crc = 0;
    for (uint16_t i=0; i<len; i++)
    {
    	uint8_t byte = data[i];
		crc ^= byte;
		for (uint16_t j=0; j<8; j++)
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x07;
			else
				crc <<= 1;
			crc &= 0xFF;
		}
    }
    return crc;
}

bool rpc_data_link_receive(rpc_context_t* rpc_context)
{
    //Restore context
    void* phy_context   = rpc_context->phy_context;
    uint8_t *state      = &rpc_context->rx_fsm_state;
    uint8_t *rx_buff    = rpc_context->rx_buff;
    uint32_t *counter   = &rpc_context->rx_buff_indx;
    uint8_t byte = 0;

    //Projection
    rpc_datalink_header_t* header = (rpc_datalink_header_t*)&rx_buff[0];

    //Update buffer
    rpc_context->rpc_phy_update_rx_buffer(phy_context);

	for(uint32_t byte_count=0; byte_count< 2*RPC_DATALAYER_PACKET_MAX_LEN; byte_count++)	//Don't let block forever
	{   
        //Get byte
        if( rpc_context->rpc_phy_read_byte(phy_context, &byte) == false)	//~0.8 us - Need optimization
			return false;
        
        //FSM WORK
		switch (*state) {

            //Wait start header byte
			case W_START_HEADER:
			{
                //Check byte
				if(byte != RPC_DATALAYER_START_HEADER)
					break;

				*state = W_START_DATA; 
				rx_buff[0] = byte;
                *counter = 1;
			}break;

			//Wait start data byte, check lenght and crc8
			case W_START_DATA:
			{
				rx_buff[*counter] = byte;
                *counter = *counter +1;

                //Wait full header
				if(*counter != RPC_DATALAYER_HEADER_LEN)
					break;

                //Check byte
				if(byte != RPC_DATALAYER_START_DATA)
				{
					*state = W_START_HEADER;
					break;
				}

				//Length cannot exceed RPC_DATALAYER_PACKET_MAX_LEN bytes
				if(header->lenght_data > RPC_DATALAYER_PACKET_MAX_LEN)
				{
					*state = W_START_HEADER;
					break;
				}

                //Crc8
                uint8_t header_crc8 = header->crc8_header;
                header->crc8_header = 0;
                if(header_crc8 != calculate_crc8(rx_buff, RPC_DATALAYER_HEADER_LEN))
                {
					*state = W_START_HEADER;
					break;
                }
                header->crc8_header = header_crc8;  //Need for calculation packet crc8

				*state = W_END_DATA;
			}break;

			//Wait stop byte, check packet crc8
			case W_END_DATA:
			{
			    rx_buff[*counter] = byte;
                *counter = *counter +1;

                //Wait full header
				if(*counter != header->lenght_data)
					break;

                //Check byte
				if(byte != RPC_DATALAYER_STOP_DATA)
				{
					*state = W_START_HEADER;
					break;
				}

                //Crc8
                rpc_datalink_tail_t* tail = (rpc_datalink_tail_t*)&rx_buff[header->lenght_data - RPC_DATALAYER_TAIL_LEN];
                uint8_t packet_crc8 = tail->crc8_packet;
                tail->crc8_packet = 0;
                if(packet_crc8 != calculate_crc8(rx_buff, header->lenght_data))
                {
					*state = W_START_HEADER;
					break;
                }
                tail->crc8_packet = packet_crc8;          
				
                *state = W_START_HEADER;
				return true;
			}break;
			default: *state = W_START_HEADER;break;
		}
	}
	return false;
}


bool rpc_data_link_transmit(rpc_context_t* rpc_context, uint16_t len)
{
    //Calc of package length
	uint16_t len_pack = len + RPC_DATALAYER_HEADER_LEN + RPC_DATALAYER_TAIL_LEN;

	/* Create packet */
    uint8_t* tx_buf = rpc_context->tx_buff;

    //Header
    rpc_datalink_header_t* header = (rpc_datalink_header_t*)&tx_buf[0];
	header->start_header = RPC_DATALAYER_START_HEADER;
	header->lenght_data  = len_pack;  //Little endian
    header->crc8_header  = 0;
    header->start_data   = RPC_DATALAYER_START_DATA;

    //Tail
    rpc_datalink_tail_t* tail = (rpc_datalink_tail_t*)&tx_buf[len_pack-RPC_DATALAYER_TAIL_LEN];
	tail->crc8_packet = 0;
	tail->stop_data   = RPC_DATALAYER_STOP_DATA;

    //Calc crc8
    header->crc8_header = calculate_crc8(tx_buf, RPC_DATALAYER_HEADER_LEN);
	tail->crc8_packet   = calculate_crc8(tx_buf, len_pack);

	//Sending
	bool ret = rpc_context->rpc_phy_write_bytes(rpc_context->phy_context, tx_buf, len_pack);
    rpc_context->rpc_phy_update_tx_buffer(rpc_context->phy_context);

    return ret;
}
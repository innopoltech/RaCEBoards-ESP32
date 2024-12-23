#include "rpc_transport.h"
#include "rpc_data_link.h"

#define RPC_TRANSPORT_LOG false

static void reset_context(rpc_context_t* rpc_context)
{
    if(rpc_context->tx_buff != NULL)
        memset(rpc_context->tx_buff, 0, sizeof(RPC_TRANSPORT_TX_BUFF_LEN));
    if(rpc_context->rx_buff != NULL)
        memset(rpc_context->rx_buff, 0, sizeof(RPC_TRANSPORT_RX_BUFF_LEN));

    rpc_context->tx_buff_indx = 0;
    rpc_context->rx_buff_indx = 0;

    rpc_context->rx_fsm_state = W_START_HEADER;
}

bool  rpc_create(rpc_context_t* rpc_context, bool use_psram)
{
    rpc_delete(rpc_context);

    //Malloc mem
    uint32_t flag = MALLOC_CAP_8BIT;
    flag |= use_psram ? MALLOC_CAP_SPIRAM : 0;

    rpc_context->tx_buff = (uint8_t*)heap_caps_malloc(RPC_TRANSPORT_TX_BUFF_LEN, flag);
    if(rpc_context->tx_buff == NULL)
        return false;

    rpc_context->rx_buff = (uint8_t*)heap_caps_malloc(RPC_TRANSPORT_RX_BUFF_LEN, flag);
    if(rpc_context->rx_buff == NULL)
        return false;

    reset_context(rpc_context);
    return true;
}

bool  rpc_delete(rpc_context_t* rpc_context)
{
    if( rpc_context->tx_buff != NULL)
        heap_caps_free(rpc_context->tx_buff);
    if( rpc_context->rx_buff != NULL)
        heap_caps_free(rpc_context->rx_buff);

    rpc_context->tx_buff = NULL;
    rpc_context->rx_buff = NULL;
    
    reset_context(rpc_context);
    return true;  
}

static bool restore_header(uint8_t *rx_buff, rpc_transport_header_t* rpc_transport_header)
{   
    uint16_t lenght_msg = ((rpc_datalink_header_t*)&rx_buff[0])->lenght_data;
    lenght_msg = lenght_msg - RPC_DATALAYER_HEADER_LEN - RPC_DATALAYER_TAIL_LEN;

    char* name_start = (char*)&rx_buff[RPC_DATALAYER_HEADER_LEN + RPC_TRANSPORT_NAME_OFFSET];

    uint8_t* packet = &rx_buff[RPC_DATALAYER_HEADER_LEN];
    uint8_t* packet_end = &rx_buff[RPC_DATALAYER_HEADER_LEN + lenght_msg - 1];
    
    //Fill msg type and number
    rpc_transport_header->msg_type = packet[0];
    rpc_transport_header->msg_counter = packet[1];

    //Find terminator
	char* terminator_ptr = (char*)memchr(name_start, 0, lenght_msg-RPC_TRANSPORT_NAME_OFFSET);
	if(terminator_ptr == NULL)
		return false;

    rpc_transport_header->name = name_start;
    rpc_transport_header->name_len = (uint16_t)(terminator_ptr - name_start + 1);   //Include terminator

    //Truncate name lenght
    if(rpc_transport_header->name_len > RPC_DICT_ELEMENT_NAME_LEN)
    {
        rpc_transport_header->name_len = RPC_DICT_ELEMENT_NAME_LEN;
        rpc_transport_header->name[RPC_DICT_ELEMENT_NAME_LEN-1] = 0;
    }

    //Data
    rpc_transport_header->data = (uint8_t*)(terminator_ptr+1);  //Data start after terminator
    rpc_transport_header->data_len = (uint16_t)(packet_end - rpc_transport_header->data + 1);

    return true;
}

static bool restore_header_from_args(uint8_t type, rpc_function_args_t* args, rpc_transport_header_t* rpc_transport_header)
{
    //Name 
	char* terminator_ptr = (char*)memchr(args->name, 0, RPC_DICT_ELEMENT_NAME_LEN);
	if(terminator_ptr == NULL)
		return false;

    rpc_transport_header->name = args->name;
    rpc_transport_header->name_len = (uint16_t)(terminator_ptr - rpc_transport_header->name + 1);   //Include terminator

    //Data
    if(type == RPC_TRANSPORT_TYPE_REQ || type == RPC_TRANSPORT_TYPE_STREAM)
    {
        rpc_transport_header->data = args->data_in;
        rpc_transport_header->data_len = args->len_in;
    }
    else if(type == RPC_TRANSPORT_TYPE_RES)
    {
        rpc_transport_header->data = args->data_out;
        rpc_transport_header->data_len = args->len_out;
    }

    return true;
}

#if RPC_USE_FREERTOS
/* ################################################ FREERTOS ################################################ */

//NOT IMPLEMENTED

#else
/* ################################################ RAW ################################################ */

bool rpc_start(rpc_context_t* rpc_context)
{
    rpc_stop(rpc_context);
    return true;
}

bool rpc_stop(rpc_context_t* rpc_context)
{
    reset_context(rpc_context);
    return true;
}

void rpc_transport_receive(rpc_context_t* rpc_context, rpc_transport_header_t* rpc_transport_header)
{
    //Check new msg
    bool new_msg = rpc_data_link_receive(rpc_context);
    if(new_msg == false)
        return;

    //Restore context
    uint8_t *rx_buff    = rpc_context->rx_buff;

    //Check data lenght
    rpc_datalink_header_t* dt_header = (rpc_datalink_header_t*)&rx_buff[0];
    int32_t lenght_msg = dt_header->lenght_data - RPC_DATALAYER_HEADER_LEN - RPC_DATALAYER_TAIL_LEN;
    if(lenght_msg < RPC_TRANSPORT_MIN_MSG_SIZE)
        return;

    //Restore header
    rpc_transport_header_t header = {0,};
    if(restore_header(rx_buff, &header) == false)
    {
        return;
    }

	//If request or stream
	if(header.msg_type == RPC_TRANSPORT_TYPE_REQ || header.msg_type == RPC_TRANSPORT_TYPE_STREAM)
	{
        //Create args
 		uint8_t ret_bool = 0;
        rpc_function_args_t rpc_function_args =
        {
            .name = header.name,
            .data_in = header.data,
            .len_in = header.data_len,
            .data_out = NULL,
            .len_out = 0,
            #if RPC_ARGS_INCLUDE_CONTEXT
            .context = (void*)rpc_context
            #endif
        };

		//Finding a function to execute
        function_t func = rpc_function_ptr_from_indx( rpc_function_find(header.name) );
		if(func != NULL)
		{
			func(&rpc_function_args);
			ret_bool = 1;
		}
		else
		{
			func = rpc_function_ptr_from_indx( rpc_function_find("default") );
			if(func != NULL)
			{
                func(&rpc_function_args);
                ret_bool = 1;
			}
		}

        if(RPC_TRANSPORT_LOG)
            printf("RPC_TRANSPORT - REC CALL, function '%s', result = %s\n", header.name, ret_bool ? "Ok":"Fail");

		//Sending the result
        if(header.msg_type == RPC_TRANSPORT_TYPE_REQ)
        {
                uint8_t ret_type = ret_bool==1 ? RPC_TRANSPORT_TYPE_RES : RPC_TRANSPORT_TYPE_ERR;
                rpc_transport_call_single(rpc_context, ret_type, header.msg_counter, &rpc_function_args);
        }
	}
	//If Responce or Error
	else if(header.msg_type == RPC_TRANSPORT_TYPE_RES || header.msg_type == RPC_TRANSPORT_TYPE_ERR)
	{   
        //Copy to external header
		if(rpc_transport_header != NULL)
            memcpy(rpc_transport_header, &header, sizeof(rpc_transport_header_t));
	}  
    //UNKNOW
    else
    {
        printf("RPC_TRANSPORT - rec msg with unknow type! Type=%d\n", header.msg_type);
    }
}

bool rpc_transport_call_single(rpc_context_t* rpc_context, uint8_t type, uint8_t id, rpc_function_args_t* args)
{
	//Create header
    rpc_transport_header_t header = {0,};
    if(restore_header_from_args(type, args, &header) == false )
    {   
        printf("RPC_TRANSPORT - impossible create header! Trouble with name!\n");
        return false;
    }   

    //Id generation or use provided id
    uint8_t gen_id = 0;
    if(type == RPC_TRANSPORT_TYPE_REQ || type == RPC_TRANSPORT_TYPE_STREAM)
    {
        gen_id = rpc_context->tx_buff_indx;
        rpc_context->tx_buff_indx = (rpc_context->tx_buff_indx + 1)%(UINT8_MAX);
    }
    else
        gen_id = id;

    /* Create packet */
    //Prepare pointers
    uint8_t* tx_buff = rpc_context->tx_buff;
    uint8_t* start_msg  = &tx_buff[RPC_DATALAYER_HEADER_LEN];
    uint8_t* start_name = &start_msg[RPC_TRANSPORT_NAME_OFFSET];
    uint8_t* terminator = &start_name[header.name_len-1];
    uint8_t* start_data = &terminator[1];
    uint8_t* end_data   = &start_data[header.data_len-1];

    //Fill type and id
	start_msg[0] = type;
	start_msg[1] = gen_id;

    //Copy name
    memcpy(start_name, header.name, header.name_len);

    //Terminator
    *terminator = 0;

    //Copy data
    if(header.data != NULL && header.data_len > 0)
        memcpy(start_data, header.data, header.data_len);

    //Calc lenght
    uint16_t lenght = (uint16_t)(end_data - start_msg + 1);
    
    if(RPC_TRANSPORT_LOG && (type == RPC_TRANSPORT_TYPE_REQ || type == RPC_TRANSPORT_TYPE_STREAM))
    {
        printf("RPC_TRANSPORT - TX CALL, function '%s\n", header.name);
    }

	if(type == RPC_TRANSPORT_TYPE_REQ)
	{   
        //Send
        bool ret = rpc_data_link_transmit(rpc_context, lenght);
        if(ret == false)
            return false;

        /* And wait response */
		uint64_t time_stop = esp_timer_get_time() + RPC_TRANSPORT_TX_TIMEOUT_MS*1000;
        int64_t time_wdt = 0;

		for(; esp_timer_get_time() < time_stop ;)
		{
            if(esp_timer_get_time() > time_wdt + 100000)    //Reset wdt timer every 100ms
            {	
                UBaseType_t my_prio = uxTaskPriorityGet(NULL);
                vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
                time_wdt = esp_timer_get_time();

                taskYIELD();
                vTaskPrioritySet(NULL, my_prio);
            }
            else
                taskYIELD();                                 //Instead of delay

            //Clear header and try receive packet
            memset(&header, 0, sizeof(rpc_transport_header_t));
			rpc_transport_receive(rpc_context, &header);

            //No msg
            if(header.msg_type == 0)
                continue;

            //Not equals id's
			if(header.msg_counter != gen_id)
				continue;

            //All ok - copy data ptr and len
            args->data_out = header.data;
            args->len_out =  header.data_len;
			
            return true;
		}
		return false;
	}
	else
	{
        //Just send
		return rpc_data_link_transmit(rpc_context, lenght);
	}
}
#endif
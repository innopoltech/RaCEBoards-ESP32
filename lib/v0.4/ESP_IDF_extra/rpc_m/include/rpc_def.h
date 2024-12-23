#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#pragma once

/* ################################################ FUNCTION ################################################ */
/*
    Все доступные для вызова функции хранятся в словаре rpc_dict_t, где каждый элемент - структура rpc_element_dict_t.
    Каждый элемент хранит ссылку на функцию и её название.
    Функция вызывается с аргументом в виде указателя на структуру rpc_function_args_t.
*/
#define RPC_DICT_LEN 16
#define RPC_DICT_ELEMENT_NAME_LEN 32

#define RPC_ARGS_INCLUDE_CONTEXT true

typedef struct
{
	char* name;
    uint8_t* data_in; 
    uint16_t len_in; 
    uint8_t* data_out;
    uint16_t len_out;
#if RPC_ARGS_INCLUDE_CONTEXT
    void* context;
#endif
}rpc_function_args_t;

typedef void (*function_t)(rpc_function_args_t* args);

typedef struct
{
	char name[RPC_DICT_ELEMENT_NAME_LEN];
	function_t function;
}rpc_element_dict_t;

typedef struct
{
	rpc_element_dict_t dict_element[RPC_DICT_LEN];
	bool empty_element[RPC_DICT_LEN];
}rpc_dict_t;

/* ################################################ PHYSICAL ################################################ */
/*
    Data link layer RPC общается с физическим уровнем через указатели на функции в контексте RPC.
*/

//Shared functions
typedef void (*rpc_phy_update_rx_buffer_t)(void* context);
typedef void (*rpc_phy_update_tx_buffer_t)(void* context);
typedef bool (*rpc_phy_read_byte_t)(void* context, uint8_t* data);
typedef bool (*rpc_phy_write_bytes_t)(void* context, uint8_t* data, uint32_t len);

/* ################################################ TRANSPORT ################################################ */
#define RPC_USE_FREERTOS false

#define RPC_TRANSPORT_TX_BUFF_LEN 512
#define RPC_TRANSPORT_RX_BUFF_LEN 512

#define RPC_TRANSPORT_NAME_OFFSET   2
#define RPC_TRANSPORT_MIN_MSG_SIZE  4

#define RPC_TRANSPORT_TX_TIMEOUT_MS 500  

#define RPC_TRANSPORT_TYPE_REQ     11
#define RPC_TRANSPORT_TYPE_STREAM  12
#define RPC_TRANSPORT_TYPE_RES     22
#define RPC_TRANSPORT_TYPE_ERR     33

typedef struct 
{
    //Auto fill
    uint8_t* rx_buff;
    uint8_t* tx_buff;

    uint32_t rx_buff_indx;
    uint32_t tx_buff_indx;

    uint8_t rx_fsm_state;

    //Manual fill
    void* phy_context;      
    rpc_phy_update_rx_buffer_t rpc_phy_update_rx_buffer;
    rpc_phy_update_tx_buffer_t rpc_phy_update_tx_buffer;
    rpc_phy_read_byte_t        rpc_phy_read_byte;
    rpc_phy_write_bytes_t      rpc_phy_write_bytes;
}rpc_context_t;

typedef struct
{
    uint8_t msg_type;
    uint8_t msg_counter;

    char* name;
    uint16_t name_len;

    uint8_t* data;
    uint16_t data_len;
}rpc_transport_header_t;


/* ################################################ DATA_LAYER ################################################ */

#define RPC_DATALAYER_PACKET_MAX_LEN  300
#define RPC_DATALAYER_HEADER_LEN      5
#define RPC_DATALAYER_TAIL_LEN        2

#define RPC_DATALAYER_START_HEADER    0xFA
#define RPC_DATALAYER_START_DATA      0xFB
#define RPC_DATALAYER_STOP_DATA       0xFE

typedef enum
{
    W_START_HEADER = 0,
    W_START_DATA,
    W_END_DATA
}rpc_datalink_fsm_states_t;

#pragma pack(push, 1)   //Don't padding
typedef struct
{
    uint8_t start_header;
    uint16_t lenght_data;
    uint8_t crc8_header;
    uint8_t start_data;
}rpc_datalink_header_t;

typedef struct
{
    uint8_t crc8_packet;
    uint8_t stop_data;
}rpc_datalink_tail_t;
#pragma pack(pop)

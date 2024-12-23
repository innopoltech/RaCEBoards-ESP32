#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "wifi_m/m_wifi.h"
#include "udp_m/m_udp.h"
#include "gatt_spp.h"

#include "rpc_def.h"
#include "rpc_phy_udp.h"
#include "rpc_phy_spp.h"
#include "rpc_function.h"
#include "rpc_transport.h"

#include <string.h>

static const char *TAG = "rpc_example";

void test_function_function(rpc_function_args_t* args)
{
    static uint16_t ret = 524;
    args->data_out = (uint8_t*)&ret;
    args->len_out = sizeof(ret);

    static uint8_t ret_big[480];
    args->data_out = ret_big;
    args->len_out = args->len_in > 480 ? 480 : args->len_in;

    //ESP_LOGI(TAG, "In function %s, arg 1=%d", args->name, *args->data_in);
}


static rpc_phy_udp_context_t rpc_phy_udp_context = {0,};
static rpc_context_t rpc_context = {0,};
void init_wifi_udp_example()
{
//Connect to wifi
    ESP_LOGI(TAG, "Connect to wifi...");
    wifi_start_wifi_task(PRO_CPU_NUM);

    wifi_connect_to_ap("INNOPOL", "Azbuka1331");
    while(wifi_get_status() != WIFI_ACTIVE){vTaskDelay(100 / portTICK_PERIOD_MS);}

    esp_ip4_addr_t ip_info = wifi_get_ip();
    ESP_LOGI(TAG, "Connected to WIFI AP! IP: " IPSTR, IP2STR(&ip_info));

//Init rpc_phy_udp
    rpc_phy_udp_create(&rpc_phy_udp_context, false);
    rpc_phy_udp_start(&rpc_phy_udp_context);

//Init rpc_transport
    rpc_context.phy_context = (void*)&rpc_phy_udp_context;
    rpc_context.rpc_phy_update_rx_buffer = rpc_phy_udp_update_rx_buffer;
    rpc_context.rpc_phy_update_tx_buffer = rpc_phy_udp_update_tx_buffer;
    rpc_context.rpc_phy_read_byte   = rpc_phy_udp_read_byte;
    rpc_context.rpc_phy_write_bytes = rpc_phy_udp_write_bytes;
}

void deinit_wifi_udp_example()
{
//Deinit rpc_phy_udp
    rpc_phy_udp_stop(&rpc_phy_udp_context);
    rpc_phy_udp_delete(&rpc_phy_udp_context);

//Disconnect from wifi
    wifi_dissconnect_from_ap();
    while(wifi_get_status() != WIFI_ILDE){vTaskDelay(100 / portTICK_PERIOD_MS);}
    wifi_stop_wifi_task();
}

static rpc_phy_spp_context_t rpc_phy_spp_context = {0,};
void init_ble_gatt_spp_example()
{
//Enable BLE_GATT_SPP
    gatt_spp_start(PRO_CPU_NUM);

//Init rpc_phy_udp
    rpc_phy_spp_create(&rpc_phy_spp_context, false);
    rpc_phy_spp_start(&rpc_phy_spp_context);

//Init rpc_transport
    rpc_context.phy_context = (void*)&rpc_phy_spp_context;
    rpc_context.rpc_phy_update_rx_buffer = rpc_phy_spp_update_rx_buffer;
    rpc_context.rpc_phy_update_tx_buffer = rpc_phy_spp_update_tx_buffer;
    rpc_context.rpc_phy_read_byte   = rpc_phy_spp_read_byte;
    rpc_context.rpc_phy_write_bytes = rpc_phy_spp_write_bytes; 
}

void deinit_ble_gatt_spp_example()
{
//Deinit rpc_phy_udp
    rpc_phy_spp_stop(&rpc_phy_spp_context);
    rpc_phy_spp_delete(&rpc_phy_spp_context);

//Disable BLE_GATT_SPP
    gatt_spp_stop();
}





void app_main(void)
{
//Wifi and UDP
    //init_wifi_example();

//BLE GATT and SPP
    init_ble_gatt_spp_example();

//Init rpc_function
    rpc_function_init();
    rpc_function_add("test_function_function", (void*)test_function_function);

    rpc_create(&rpc_context, false);
    rpc_start(&rpc_context);

    int64_t time_wdt = 0;
    int64_t time_call = 0;

    while(1)
    {
        //Rec
        rpc_transport_receive(&rpc_context, NULL);

        //Call
        // if(esp_timer_get_time() > time_call)
        // {
        //     uint8_t in[32];
        //     uint16_t in_len = 0;
        //     in_len = sprintf((char*)in, "kak tak to...\n");

        //     rpc_function_args_t args = 
        //     {
        //         .name =  "proverochka_1",
        //         .data_in = in,
        //         .len_in = in_len,
        //         .data_out = NULL,
        //         .len_out = 0
        //     };
        //     bool ret = rpc_transport_call_single(&rpc_context, RPC_TRANSPORT_TYPE_REQ, 0, &args);
        //     if(ret)
        //     {
        //         //((char*)(args.data_out))[args.len_out] = 0;    //Manual add null-terminator if needed
        //         ESP_LOGI(TAG, "Called function res=%d; try_res = %s", ret, (char*)(args.data_out));
        //     }
        //     else
        //         ESP_LOGI(TAG, "Called function res=%d", ret);

        //     time_call = esp_timer_get_time() + 3*1000*1000; //Every 3 s
        // }

        //Delay
        if(esp_timer_get_time() > time_wdt + 100000)        //Reset wdt timer every 100ms
		{	
			UBaseType_t my_prio = uxTaskPriorityGet(NULL);
			vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
			taskYIELD();
			vTaskPrioritySet(NULL, my_prio);
			time_wdt = esp_timer_get_time();
		}
        else
            taskYIELD();                                    //Instead of delay
        //vTaskDelay(10/portTICK_PERIOD_MS);                //Or just simple vTaskDelay
    }

//Deinit rpc_transport
    rpc_stop(&rpc_context);
    rpc_delete(&rpc_context);

//Deinit rpc_function
   rpc_function_delete("test_function_function");
   rpc_function_deinit();

//Wifi and UDP
    //deinit_wifi_udp_example();

//BLE GATT and SPP
    deinit_ble_gatt_spp_example();
}

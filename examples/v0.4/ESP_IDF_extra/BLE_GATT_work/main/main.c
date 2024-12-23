


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gatt_spp.h"
static const char *TAG = "BLE_GATT_work";

void app_main(void)
{   
    //Prepare block
    m_spp_data_block_t data_block = {0,};

    //Start ble_gatt_spp
    gatt_spp_start(PRO_CPU_NUM);
    while(1)
    {
        //Check receive msg
        vTaskDelay(1); 
        if(gatt_spp_copy_rx(&data_block) != ESP_OK)
            continue;

        data_block.data[data_block.length] = 0;
        ESP_LOGI(TAG, "Rec msg = '%s'", (char*)data_block.data);

        //Send msg
        data_block.length = sprintf( (char*)data_block.data, "Ok, send echo!Ok, send echo!Ok, send echo!Ok, send echo!");
        ESP_LOGI(TAG, "Send msg = '%s'", (char*)data_block.data);

        gatt_spp_request_send(&data_block);
    }
}
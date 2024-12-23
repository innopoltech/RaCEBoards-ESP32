


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gatt_spp.h"
#include "gatt_spp_c.h"

#include "esp_mac.h"
static const char *TAG = "BLE_GATT_C_work";

void app_main(void)
{   
    // uint8_t mac[6];
    // esp_read_mac(mac, ESP_MAC_BT);
    // printf("Public Device Address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Power on!");

    //Prepare block
    m_spp_c_data_block_t data_block = {0,};
    m_spp_c_data_block_t data_block2 = {0,};

    //Start ble_gatt_spp
    gatt_spp_c_start(PRO_CPU_NUM);
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000)); 

        //Send msg
        data_block.indx = 0; 
        data_block.length = sprintf( (char*)data_block.data, "Test connect client send!");
        ESP_LOGI(TAG, "Send msg = '%s'", (char*)data_block.data);
        gatt_spp_c_request_send(&data_block);

        //Wait answer
        data_block2.indx = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
        if( gatt_spp_c_copy_rx(&data_block2) != ESP_OK)
            continue;
        
        data_block2.data[data_block2.length] = 0;
        ESP_LOGI(TAG, "Rec msg [%d] = '%s'", data_block2.indx, (char*)data_block2.data);
    }

    // //Prepare block
    // m_spp_data_block_t data_block = {0,};
    // m_spp_data_block_t data_block2 = {0,};
    // //Start ble_gatt_spp
    // gatt_spp_start(PRO_CPU_NUM);
    // while(1)
    // {
    //     //Check receive msg
    //     vTaskDelay(1); 
    //     if(gatt_spp_copy_rx(&data_block) != ESP_OK)
    //         continue;

    //     data_block.data[data_block.length] = 0;
    //     ESP_LOGI(TAG, "Rec msg = '%s'", (char*)data_block.data);

    //     //Send msg
    //     data_block2.length = snprintf( (char*)data_block2.data, sizeof(data_block.data)-30, "Ok, send echo =  %s", data_block.data);
    //     ESP_LOGI(TAG, "Send msg = '%s'", (char*)data_block2.data);

    //     gatt_spp_request_send(&data_block2);
    // }
}
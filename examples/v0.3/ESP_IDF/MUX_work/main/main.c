#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "m_mux.h"
/*
    PERIPHERALS USED:
        * ADC 1
*/
static const char *TAG = "MUX_work";

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to MUX!");

    //Calibrate adc
    adc_init();

    //Config mux pin
    init_mux();

    int raw_volt = 0;
    int volt     = 0;

    //Work
    while (1) {
        //Switch pin mode
        mux_to_analog();

        //Select Line 
        mux_select_line(0);

        //Measure and calculate battery voltage
        raw_volt = mux_get_analog();
        volt    = raw_volt*2;

        ESP_LOGI(TAG, "Test Analog Line 0");
        ESP_LOGI(TAG, "My_batttery - In raw: %d mV:, With res div: %d mV",raw_volt,volt);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

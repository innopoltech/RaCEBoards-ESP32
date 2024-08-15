#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "m_spi_thread.h"
#include "m_ra01s.h"
#include "esp_log.h"

//NOTE : if you need to use sdcard, it must be initialized first!
#define SDCARD_CS_PIN 15

#define TEST_TX     true
#define IS_STRING   false

static const char *TAG = "Ra01S_work";

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to spi Ra01S_TX!");

    //CS-HIGH for sdcard
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(SDCARD_CS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(SDCARD_CS_PIN, 1);

    //Create SPI
    spi_t_create();

    //Radio init
    ra01s_init(spi_t_get_host());
    ra01s_setMaxPower();
    ra01s_setChannel(1);

    uint8_t msg_len;
    char msg[255];

    while(1)
    {   
        #if TEST_TX
            #if IS_STRING
                msg_len = sprintf(msg, "Hello BLOCK WORLD!!!\n");
                ra01s_sendS(msg, msg_len);
            #else
                ra01s_sendTelemetryPack(0, 0, 1, 512, -5, 45.123, 33.456);
            #endif
        #else   
            static int num_string_msg = 0;
            (void)num_string_msg;
            (void)msg_len;
            bool has_msg = false;
            ra01s_availablePacket(&has_msg);

            #if IS_STRING
                if(has_msg)
                {
                    ra01s_reciveS(msg, ((uint8_t)sizeof(msg)));
                    ESP_LOGI(TAG, "Number: %d, Message: %s", num_string_msg++, msg);             
                }
            #else
                if(has_msg)
                {
                    ra01s_parceTelemetryPack(msg, sizeof(msg));
                    ESP_LOGI(TAG, "%s" , msg);
                }
            #endif
        #endif

        //ESP_LOGI(TAG, "ILDE");
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ra01s_deinit();

    ESP_LOGI(TAG, "All done!");
}

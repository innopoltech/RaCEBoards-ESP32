#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "m_gnss.h"

#include <stdio.h>

static const char *TAG = "GNSS_work";


void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to GNSS!");

    bool res;
    gnss_init(9600);

    char buf[128] = {0};
    //int buf_len = sprintf(buf, "PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");    //Turn on the basic GGA and RMC info (default)
    //int buf_len = sprintf(buf, "PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");    //Turn on just minimum info (RMC only, location):
    //int buf_len = sprintf(buf, "PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");    //Turn off everything:
    int buf_len = sprintf(buf, "PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0");      //Turn on everything (not all of it is parsed!)
    gnss_send_command((uint8_t*)buf,buf_len);

    //Work
    uint64_t last_print = 0;
    while (1) 
    {
        vTaskDelay(20 / portTICK_PERIOD_MS);    
        gnss_update(&res);

        uint64_t current = esp_timer_get_time()/1000;

        if (current - last_print >= 700)
        {
            last_print = current;
            //no valid data
            if ( !(m_gnss_locals.fix_quality != 0 && m_gnss_locals.fix_quality >= 1))
            {   
                ESP_LOGI(TAG, "Waiting for fix... (last raw nmea: %s )", m_gnss_locals._raw_sentence_print);

                continue;
            }

            //valid data, print everything
            ESP_LOGI(TAG, "=========================================");
            ESP_LOGI(TAG, "Fix timestamp: %d/%d/%d %02d:%02d:%02d",
                    m_gnss_locals.timestamp_utc.tm_mon, 
                    m_gnss_locals.timestamp_utc.tm_mday, 
                    m_gnss_locals.timestamp_utc.tm_year,
                    m_gnss_locals.timestamp_utc.tm_hour,
                    m_gnss_locals.timestamp_utc.tm_min,
                    m_gnss_locals.timestamp_utc.tm_sec);

            ESP_LOGI(TAG, "Latitude: %3.06f degrees"  , m_gnss_locals.latitude);
            ESP_LOGI(TAG, "Longitude: %3.06f degrees" , m_gnss_locals.longitude);

            ESP_LOGI(TAG, "Precise Latitude: %d.%d degrees" , m_gnss_locals.latitude_degrees, m_gnss_locals.latitude_minutes);
            ESP_LOGI(TAG, "Precise Longitude: %d.%d degrees" , m_gnss_locals.longitude_degrees, m_gnss_locals.longitude_minutes);
            ESP_LOGI(TAG, "Fix quality: %d" , m_gnss_locals.fix_quality);

            ESP_LOGI(TAG, "Satellites: %d" , m_gnss_locals.satellites);
            ESP_LOGI(TAG, "Altitude: %.2f meters" , m_gnss_locals.altitude_m);
            ESP_LOGI(TAG, "Speed: %.2f knots" , m_gnss_locals.speed_knots);
            ESP_LOGI(TAG, "Track angle: %.2f degrees" , m_gnss_locals.track_angle_deg);
            ESP_LOGI(TAG, "Horizontal dilution: %.2f" , m_gnss_locals.horizontal_dilution);
            ESP_LOGI(TAG, "Height geoid: %.2f meters" , m_gnss_locals.height_geoid);
        }
    
    }

}

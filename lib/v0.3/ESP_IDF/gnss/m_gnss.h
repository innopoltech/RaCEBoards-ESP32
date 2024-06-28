
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#pragma once


#define GNSS_ENABLE_INFO         false

typedef struct
{
    uint16_t tm_year;
    uint16_t tm_mon;
    uint16_t tm_mday;
    uint16_t tm_hour;
    uint16_t tm_min;
    uint16_t tm_sec;
    uint16_t tm_wday;
    uint16_t tm_yday;
    uint8_t tm_isdst;
}m_gnss_time_t;



typedef struct
{
    m_gnss_time_t timestamp_utc;

    float latitude;
    int latitude_degrees;
    int latitude_minutes;
    float longitude;
    int longitude_degrees;
    int longitude_minutes;

    int fix_quality;
    int fix_quality_3d;

    int satellites;
    int satellites_prev;

    float horizontal_dilution;
    float altitude_m;
    float height_geoid;
    float speed_knots;
    float track_angle_deg;

    char isactivedata;
    char sel_mode;
    float pdop;
    float hdop;
    float vdop;

    int total_mess_num;
    int mess_num;

    //helpres
    char arg_list[20][25];
    uint8_t arg_len;

    uint32_t* param_list[20];
    char param_list_char[20];
    char param_list_string[20][25];
    int param_list_int[20];
    float param_list_float[20];

    char _raw_sentence[256];
    char _raw_sentence_print[256];
    char _data_type[32];

    char _talker[16];
    char _sentence_type[16];

}m_gnss_locals_t;

extern m_gnss_locals_t m_gnss_locals;


esp_err_t gnss_init(int baudrate);
esp_err_t gnss_deinit();


esp_err_t gnss_update(bool* result);
esp_err_t gnss_send_command(uint8_t* command, uint8_t len);
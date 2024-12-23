
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "queue.h"

#include "tinyusb.h"
#include "tusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#pragma once

//Need activate in menuconfig - usb-cdc
//idf_component.yml ->   espressif/esp_tinyusb: "~1.4.5"
//  					   espressif/tinyusb: "~0.15.0"
#define M_USB_CDC_MSG_MAX_SIZE 256
#define M_USB_CDC_RX_MAX_SIZE  4096

typedef struct 
{
    uint8_t data[M_USB_CDC_MSG_MAX_SIZE];
    uint16_t data_len;
}m_usb_cdc_datablock_t;

typedef struct 
{
    uint8_t data[M_USB_CDC_RX_MAX_SIZE];
    uint32_t ptr_int;
    uint32_t ptr_ext;
    uint32_t read_a;
    uint32_t write_a;
}m_usb_cdc_cicrcular_t;

void m_usb_cdc_init(void);
void m_usb_cdc_deinit(void);    //Properly not work (restart fails)

void m_usb_cdc_send_data(uint8_t* data, uint32_t data_len);
uint32_t m_usb_cdc_rec_data(uint8_t* data, uint32_t data_len);
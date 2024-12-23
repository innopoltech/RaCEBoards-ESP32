#include "m_cdc_usb.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static bool usb_cdc_task_enable = false;
static TaskHandle_t usb_cdc_task = NULL;

static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_mutex = NULL;
static m_usb_cdc_cicrcular_t rx_cic = {0,};

static const char *TAG = "m_usb_cdc_lib";

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    static uint8_t buff[M_USB_CDC_MSG_MAX_SIZE] = {0,};

    //Block
    xSemaphoreTake(rx_mutex, portMAX_DELAY);
    uint32_t max_read_size= MIN(M_USB_CDC_MSG_MAX_SIZE, rx_cic.write_a);

    //Read data
    uint32_t rec_byte = 0;
    size_t rx_size = 0;
    esp_err_t ret = tinyusb_cdcacm_read(itf, buff, max_read_size, &rx_size);
    if (ret == ESP_OK) 
    {   
        while(rx_size)
        {
            //Copy byte
            rx_cic.data[rx_cic.ptr_int] = buff[rec_byte];
            
            //Ptr
            rx_cic.ptr_int=  (rx_cic.ptr_int+1) % M_USB_CDC_RX_MAX_SIZE;
            rec_byte++;

            //Len
            rx_size--;
            rx_cic.read_a++;
            rx_cic.write_a--;
        }
    }
    xSemaphoreGive(rx_mutex);
}

static void usb_cdc_send(void *pvParameters)
{   
    //Extract queue
    QueueHandle_t* queue = (QueueHandle_t*)pvParameters;
    
    m_usb_cdc_datablock_t datablock = {0,};
    while(usb_cdc_task_enable)
    {
        //Wait datablock
        if( xQueueReceive(*queue, (void*)&datablock, pdMS_TO_TICKS(50)) == pdPASS)
        {
            tinyusb_cdcacm_write_queue(TINYUSB_USBDEV_0, datablock.data, datablock.data_len);
            tinyusb_cdcacm_write_flush(TINYUSB_USBDEV_0, 0);
        }
    }

    //Close task
    usb_cdc_task = NULL;
    vTaskDelete(NULL);
}

void m_usb_cdc_init(void)
{
    if(usb_cdc_task != NULL)
    {
        ESP_LOGW(TAG, "USB task already initializated!");
        return;
    }

    //USB CDC
    ESP_LOGI(TAG, "USB initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");

    //Create queue and mutex
    tx_queue = xQueueCreate(10, sizeof(m_usb_cdc_datablock_t));
    rx_mutex = xSemaphoreCreateMutex();

    //Reset rx cic
    memset(&rx_cic, 0, sizeof(rx_cic));
    rx_cic.write_a = M_USB_CDC_RX_MAX_SIZE;

    //Create task
    usb_cdc_task_enable = true;
    xTaskCreate(usb_cdc_send, "usb_cdc_send", 16048, (void *const)&tx_queue, 5, &usb_cdc_task);
}

void m_usb_cdc_deinit(void)
{
    if(usb_cdc_task == NULL)
    {
        ESP_LOGW(TAG, "USB task already deinitializated!");
        return;
    }

    //Close task
    usb_cdc_task_enable = false;
    while(usb_cdc_task != NULL)
        vTaskDelay(1);

    //Remove
    vQueueDelete(tx_queue);
    tx_queue = NULL;
    vSemaphoreDelete(rx_mutex);
    rx_mutex = NULL;

    tusb_cdc_acm_deinit(TINYUSB_CDC_ACM_0);
    tinyusb_driver_uninstall();

    ESP_LOGI(TAG, "USB deinit DONE");
}

void m_usb_cdc_send_data(uint8_t* data, uint32_t data_len)
{
    if(usb_cdc_task == NULL)
    {
        ESP_LOGW(TAG, "USB task is not initializated!");
        return;
    }

    if(tx_queue == NULL)
    {
        ESP_LOGW(TAG, "USB TX queue not exist!");
        return;
    }

    uint32_t send_byte = 0;
    while(send_byte < data_len)
    {
        uint32_t max_size = MIN(data_len, M_USB_CDC_MSG_MAX_SIZE);

        //Copy
        m_usb_cdc_datablock_t datablock;
        datablock.data_len = max_size;
        memcpy(datablock.data, &data[send_byte], max_size);

        //Send
        xQueueSend(tx_queue, (void*)&datablock, 10);
        send_byte += max_size;
    }
}

uint32_t m_usb_cdc_rec_data(uint8_t* data, uint32_t data_len)
{
    if(usb_cdc_task == NULL)
    {
        ESP_LOGW(TAG, "USB task is not initializated!");
        return 0;
    }
    
    if(rx_mutex == NULL)
    {
        ESP_LOGW(TAG, "USB RX mutex not exist!");
        return 0 ;
    }

    //Block
    uint32_t rec_byte = 0;
    xSemaphoreTake(rx_mutex, portMAX_DELAY);
    while(data_len && rx_cic.read_a)
    {
        //Copy byte
        data[rec_byte] = rx_cic.data[rx_cic.ptr_ext];
         
        //prt
        rx_cic.ptr_ext=  (rx_cic.ptr_ext+1) % M_USB_CDC_RX_MAX_SIZE;
        rec_byte++;

        //len
        data_len--;
        rx_cic.read_a--;
        rx_cic.write_a++;
    }
    xSemaphoreGive(rx_mutex);

    //Return 
    return rec_byte;
}
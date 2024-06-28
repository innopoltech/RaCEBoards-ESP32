#include "m_spi_thread.h"

static const char *TAG = "SPI_thread_lib";

static spi_bus_config_t bus_cfg;
static sdmmc_host_t host = SDSPI_HOST_DEFAULT();

static uint8_t spi_t_created = false;

static SemaphoreHandle_t spi_t_main_semaphore = NULL;

sdmmc_host_t* spi_t_get_host()
{
    return &host;
}


esp_err_t spi_t_create()
{
    if(spi_t_created)
    {
        #ifdef SPI_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI already has been initializated!");
        #endif
        return ESP_FAIL;
    }

    #if SPI_ENABLE_INFO
        ESP_LOGI(TAG, "Initializing SPI peripheral");
    #endif

    //Config SPI bus
    host.max_freq_khz = 10000;
    bus_cfg.mosi_io_num = PIN_NUM_MOSI;
    bus_cfg.miso_io_num = PIN_NUM_MISO;
    bus_cfg.sclk_io_num = PIN_NUM_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;
    
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        #if SPI_ENABLE_ERROR
            ESP_LOGE(TAG, "Failed to initialize spi bus");
        #endif
        return ESP_FAIL;
    }

    spi_t_main_semaphore = xSemaphoreCreateMutex();
    spi_t_created = true;

    #ifdef SPI_ENABLE_INFO
        ESP_LOGI(TAG, "SPI has been successfully initialized");
    #endif

    return ESP_OK;
}

esp_err_t spi_t_close()
{
    if(spi_t_created)
        spi_bus_free(host.slot);

    spi_t_created = false;
    return ESP_OK;
}


BaseType_t spi_t_take_sem(uint32_t ms_to_wait)
{
    BaseType_t ret;

    if(ms_to_wait == 0)
        ret = xSemaphoreTake(spi_t_main_semaphore, 2);  //0-just check, 1-infinity block
    else 
        ret = xSemaphoreTake(spi_t_main_semaphore, ( TickType_t )pdMS_TO_TICKS(ms_to_wait));
   
    if(ret != pdPASS)
    {
        #ifdef SPI_ENABLE_ERROR
            ESP_LOGE(TAG, "Semaphore access error (take)!");
        #endif
    }
    return ret;
}
esp_err_t spi_t_give_sem()
{
    BaseType_t ret = xSemaphoreGive(spi_t_main_semaphore);
    if(ret != pdPASS)
    {
        #ifdef SPI_ENABLE_ERROR
            ESP_LOGE(TAG, "Semaphore access error (give)!");
        #endif
    }
    return ret;
}
#include "m_i2c_thread.h"

static const char *TAG = "I2C_thread_lib";

uint8_t i2c_t_created = false;


i2c_config_t i2c_t_conf;


//portMUX_TYPE i2c_t_spinlock = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t i2c_t_main_semaphore = NULL;



esp_err_t i2c_t_create()
{
    if(i2c_t_created)
    {
        #ifdef I2C_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C already has been initializated!");
        #endif
        return ESP_FAIL;
    }
    
    //taskENTER_CRITICAL(&i2c_t_spinlock);

    i2c_t_conf.mode = I2C_MODE_MASTER;
    i2c_t_conf.sda_io_num = I2C_MASTER_SDA;
    i2c_t_conf.scl_io_num = I2C_MASTER_SCL;
    i2c_t_conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    i2c_t_conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    i2c_t_conf.master.clk_speed = 100000;

    i2c_param_config(0, &i2c_t_conf);

    esp_err_t ret = i2c_driver_install(0, i2c_t_conf.mode, 0, 0, 0);
    if(ret != ESP_OK)
    {
        //taskEXIT_CRITICAL(&i2c_t_spinlock);
        #ifdef I2C_ENABLE_ERROR
            ESP_LOGE(TAG, "Error during i2c initialization!");
        #endif

        return ESP_FAIL;
    }

    i2c_t_main_semaphore = xSemaphoreCreateMutex();
    i2c_t_created = true;
    //taskEXIT_CRITICAL(&i2c_t_spinlock);

    #ifdef I2C_ENABLE_INFO
        ESP_LOGI(TAG, "I2C has been successfully initialized");
    #endif


    return ESP_OK;
}

esp_err_t i2c_t_close()
{
    if(i2c_t_created)
        i2c_driver_delete(0);

    i2c_t_created = false;
    return ESP_OK;
}


BaseType_t i2c_t_take_sem(uint32_t ms_to_wait)
{
    BaseType_t ret;

    if(ms_to_wait == 0)
        ret = xSemaphoreTake(i2c_t_main_semaphore, 2);  //0-just check, 1-infinity block
    else 
        ret = xSemaphoreTake(i2c_t_main_semaphore, ( TickType_t )pdMS_TO_TICKS(ms_to_wait));
   
    if(ret != pdPASS)
    {
        #ifdef I2C_ENABLE_ERROR
            ESP_LOGE(TAG, "Semaphore access error (take)!");
        #endif
    }
    return ret;
}
esp_err_t i2c_t_give_sem()
{
    BaseType_t ret = xSemaphoreGive(i2c_t_main_semaphore);
    if(ret != pdPASS)
    {
        #ifdef I2C_ENABLE_ERROR
            ESP_LOGE(TAG, "Semaphore access error (give)!");
        #endif
    }
    return ret;
}
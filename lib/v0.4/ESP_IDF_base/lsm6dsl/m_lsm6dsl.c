#include "m_lsm6dsl.h"
#include <inttypes.h>
#include <esp_log.h>

static const char *TAG = "lsm6dsl";

//Register
#define LSM6DSL_WHOAMI          0xF
#define LSM6DSL_CTRL1_XL        0x10
#define LSM6DSL_CTRL2_G         0x11
#define LSM6DSL_CTRL3_C         0x12

#define LSM6DSL_OUT_TEMP_L      0x20
#define LSM6DSL_OUT_TEMP_H      0x21

#define LSM6DSL_MILLI_G_TO_ACCEL        0.00980665
#define LSM6DSL_TEMPERATURE_SENSITIVITY 256
#define LSM6DSL_TEMPERATURE_OFFSET      25.0

#define LSM6DSL_OUTX_L_G        0x22
#define LSM6DSL_OUTX_H_G        0x23
#define LSM6DSL_OUTY_L_G        0x24
#define LSM6DSL_OUTY_H_G        0x25
#define LSM6DSL_OUTZ_L_G        0x26
#define LSM6DSL_OUTZ_H_G        0x27

#define LSM6DSL_OUTX_L_XL       0x28
#define LSM6DSL_OUTX_H_XL       0x29
#define LSM6DSL_OUTY_L_XL       0x2A
#define LSM6DSL_OUTY_H_XL       0x2B
#define LSM6DSL_OUTZ_L_XL       0x2C
#define LSM6DSL_OUTZ_H_XL       0x2D

// ...LSM6DSL_RATE_
//static float lsm6dls_rate[12] = {0, 12.5, 26.0, 52.0, 104.0, 208.0, 416.0, 833.0, 1666.0, 3332.0, 6664.0, 1.6};

// ...LSM6DSL_RANGE_
//static uint16_t lsm6dls_gyro_range_0[5] = {125, 250, 500, 1000, 2000};
static float lsm6dls_gyro_range_1[5] = {4.375, 8.75, 17.50, 35.0, 70.0};

//  ...LSM6DSL_RANGE_
//static uint16_t lsm6dls_acc_range_0[4] = {2, 16, 4, 8};
static float lsm6dls_acc_range_1[4] = {0.061, 0.488, 0.122, 0.244};

esp_err_t LSM6DSL_reset(lsm6dsl_t *dev);

void LSM6DSL_read_register_sequence(lsm6dsl_t* dev, uint8_t reg_addr, uint8_t* buf, uint8_t len)
{
    esp_err_t ret = i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, buf, len, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "ReadSequence error!");
        #endif
        return;
    }
}
uint8_t LSM6DSL_read_register8(lsm6dsl_t* dev, uint8_t reg_addr)
{
    uint8_t value;
    esp_err_t ret =  i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, &value, 1, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Read8 error!");
        #endif
        return 0;
    }

    return value;
}

void LSM6DSL_write_register8(lsm6dsl_t* dev, uint8_t reg_addr, uint8_t data)
{
    uint8_t value[2] = {reg_addr, data};
    esp_err_t ret =  i2c_master_write_to_device(0, dev->addr, value, 2,  1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Write8 error!");
        #endif
    }
}



esp_err_t lsm6dsl_init(lsm6dsl_t *dev)
{
    //Check pointers
    if(dev == NULL)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev!");
        #endif
        return ESP_FAIL;
    }

    //Check args
    if( dev->rate > LSM6DSL_RATE_6_66K_HZ || dev->acc_range > LSM6DSL_RANGE_16G || dev->gyro_range > LSM6DSL_RANGE_2000_DPS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid rate or acc_range or gyro_range!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read device id
    uint8_t data_ret8 = 0;
    data_ret8 = LSM6DSL_read_register8(dev, LSM6DSL_WHOAMI);

    //Check device id
    if(data_ret8 != dev->id)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Chip ID not match!");
        #endif
        i2c_t_give_sem();
        return ESP_FAIL;
    }

    LSM6DSL_reset(dev);

    //BDU enable
    uint8_t buf = LSM6DSL_read_register8(dev, LSM6DSL_CTRL3_C); 
    buf |=  0x40  ;
    LSM6DSL_write_register8(dev, LSM6DSL_CTRL3_C, buf);

    buf = LSM6DSL_read_register8(dev, LSM6DSL_CTRL1_XL); 
    buf &= ~0xFC;                                //Reset acc odr, range
    buf |= (dev->rate<<4);                       //Set acc odr
    buf |= (dev->acc_range<<2);                  //Set acc range
    LSM6DSL_write_register8(dev, LSM6DSL_CTRL1_XL, buf);

    buf = LSM6DSL_read_register8(dev, LSM6DSL_CTRL2_G); 
    buf &= ~0xFC;                                //Reset gyro odr, range
    buf |= (dev->rate<<4);                       //Set gyro odr
    buf |= (dev->gyro_range<<2);                 //Set gyro range
    LSM6DSL_write_register8(dev, LSM6DSL_CTRL2_G, buf);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t LSM6DSL_reset(lsm6dsl_t *dev)
{
    //Reset
    uint8_t buf = LSM6DSL_read_register8(dev, LSM6DSL_CTRL3_C); 
    buf |= 1;
    LSM6DSL_write_register8(dev, LSM6DSL_CTRL3_C, buf);

    //Wait resets
    uint64_t time_start = esp_timer_get_time();
    while(LSM6DSL_read_register8(dev, LSM6DSL_CTRL3_C) & 1)
    {
        if(esp_timer_get_time() > time_start+100000)    //100ms
        {
            i2c_t_give_sem();
            return ESP_FAIL;
        }
        vTaskDelay(2);
    }

    return ESP_OK;
}

esp_err_t lsm6dsl_acceleration(lsm6dsl_t *dev, float* data)
{
    //Check pointers
    if(dev == NULL || data == NULL)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or data!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }
    
    //Read lbs
    uint8_t lsb[6]={0};
    LSM6DSL_read_register_sequence(dev, LSM6DSL_OUTX_L_XL, (uint8_t*)lsb, 6);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Create data
    uint16_t raw;
    float acc_range = lsm6dls_acc_range_1[dev->acc_range];
    for(uint8_t i=0; i<3; i++)
    {
        raw = (lsb[i*2]  | (lsb[i*2+1]<< 8));
        data[i] = (int16_t)raw * acc_range * LSM6DSL_MILLI_G_TO_ACCEL;
    }

    return ESP_OK;
}

esp_err_t lsm6dsl_gyro(lsm6dsl_t *dev, float* data)
{
    //Check pointers
    if(dev == NULL || data == NULL)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or data!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }
    
    //Read lbs
    uint8_t lsb[6]={0};
    LSM6DSL_read_register_sequence(dev, LSM6DSL_OUTX_L_G, (uint8_t*)lsb, 6);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Create data
    uint16_t raw;
    float gyro_range = lsm6dls_gyro_range_1[dev->acc_range];
    for(uint8_t i=0; i<3; i++)
    {
        raw = (lsb[i*2]  | (lsb[i*2+1]<< 8));
        data[i] = (int16_t)raw * gyro_range / 1000.0;
    }

    return ESP_OK;
}

esp_err_t lsm6dsl_temperature(lsm6dsl_t *dev, float* data)
{
    //Check pointers
    if(dev == NULL || data == NULL)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or data!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read lbs
    uint8_t bytes_[2]={0};
    LSM6DSL_read_register_sequence(dev, LSM6DSL_OUT_TEMP_L, (uint8_t*)bytes_, 2);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if LSM6DSL_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Create data
    uint16_t raw;
    raw = (bytes_[0]  | (bytes_[1]<< 8));
    *data = (int16_t)raw/LSM6DSL_TEMPERATURE_SENSITIVITY + LSM6DSL_TEMPERATURE_OFFSET;

    return ESP_OK;
}
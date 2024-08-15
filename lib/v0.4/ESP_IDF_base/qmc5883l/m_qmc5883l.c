#include "m_qmc5883l.h"
#include <inttypes.h>
#include <esp_log.h>


#define REG_XOUT_L 0x00
#define REG_XOUT_H 0x01
#define REG_YOUT_L 0x02
#define REG_YOUT_H 0x03
#define REG_ZOUT_L 0x04
#define REG_ZOUT_H 0x05
#define REG_STATE  0x06
#define REG_TOUT_L 0x07
#define REG_TOUT_H 0x08
#define REG_CTRL1  0x09
#define REG_CTRL2  0x0a
#define REG_FBR    0x0b
#define REG_ID     0x0d

#define MASK_MODE  0xfe
#define MASK_ODR   0xf3

static const char *TAG = "qmc5883l";


uint8_t qmc5883l_read_register8(qmc5883l_t* dev, uint8_t reg_addr)
{
    uint8_t value;
    esp_err_t ret =  i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, &value, 1, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Read8 error!");
        #endif
        return 0;
    }

    return value;
}

void qmc5883l_read_register_sequence(qmc5883l_t* dev, uint8_t reg_addr, uint8_t* buf, uint8_t len)
{
    esp_err_t ret = i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, buf, len, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "ReadSequence error!");
        #endif
        return;
    }
}

void qmc5883l_write_register8(qmc5883l_t* dev, uint8_t reg_addr, uint8_t data)
{
    uint8_t value[2] = {reg_addr, data};
    esp_err_t ret =  i2c_master_write_to_device(0, dev->addr, value, 2,  1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Write8 error!");
        #endif
    }
}

esp_err_t qmc5883l_init(qmc5883l_t *dev, qmc5883l_odr_t odr, qmc5883l_osr_t osr, qmc5883l_range_t rng)
{
    //Check pointers
    if(dev == NULL)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev!");
        #endif
        return ESP_FAIL;
    }

    //Check args
    if(odr > QMC5883L_DR_200 || osr > QMC5883L_OSR_512 || rng > QMC5883L_RNG_8)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid odr or sdr or rng!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read device id
    uint8_t data_ret8 = 0;
    data_ret8 = qmc5883l_read_register8(dev, REG_ID);

    //Check device id
    if(data_ret8 != dev->id)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Chip ID not match!");
        #endif
        i2c_t_give_sem();
        return ESP_FAIL;
    }

    //Write config to device
    dev->range = rng;

    //Set/reset period
    qmc5883l_write_register8(dev, REG_FBR, 1);

    //Data rate, sample rate, range
    data_ret8 = (0x01) | ((odr & 3) << 2) | ((rng & 1) << 4) | ((osr & 3) << 6);
    qmc5883l_write_register8(dev, REG_CTRL1, data_ret8);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t qmc5883l_reset(qmc5883l_t *dev)
{
    //Check pointers
    if(dev == NULL)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev!");
        #endif
        return ESP_FAIL;
    }

    //Reset
    qmc5883l_write_register8(dev, REG_CTRL2, 0x80);
    dev->range = QMC5883L_RNG_2;

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t qmc5883l_get_raw_data(qmc5883l_t *dev, qmc5883l_raw_data_t *raw)
{
    //Check pointers
    if(dev == NULL || raw == NULL)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or raw!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }
    
    //Read lbs
    qmc5883l_read_register_sequence(dev, REG_XOUT_L, (uint8_t*)raw, 6);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Create raw data
    *((int16_t*)&raw->x) = ((uint8_t*)&raw->x)[0]  | ((uint8_t*)&raw->x)[1]<< 8;
    *((int16_t*)&raw->y) = ((uint8_t*)&raw->y)[0]  | ((uint8_t*)&raw->y)[1]<< 8;
    *((int16_t*)&raw->z) = ((uint8_t*)&raw->z)[0]  | ((uint8_t*)&raw->z)[1]<< 8;

    return ESP_OK;
}

esp_err_t qmc5883l_raw_to_mg(qmc5883l_t *dev, qmc5883l_raw_data_t *raw, float *data)
{
    //Check pointers
    if(dev == NULL || raw == NULL || data == NULL)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or raw or data!");
        #endif
        return ESP_FAIL;
    }

    float f = (dev->range == QMC5883L_RNG_2 ? 2000.0 : 8000.0) / 32768;

    data[0] = raw->x * f;
    data[1] = raw->y * f;
    data[2] = raw->z * f;

    return ESP_OK;
}

esp_err_t qmc5883l_get_data(qmc5883l_t *dev, float *data)
{
    qmc5883l_raw_data_t raw;
    qmc5883l_get_raw_data(dev, &raw);
    return qmc5883l_raw_to_mg(dev, &raw, data);
}

esp_err_t qmc5883l_get_temp(qmc5883l_t *dev, float *temp)
{
    //Check pointers
    if(dev == NULL || temp == NULL)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or temp!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read lsb
    int16_t temp_ = 0;
    qmc5883l_read_register_sequence(dev, REG_TOUT_L, (uint8_t*)&temp_, 2);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if QMC5883L_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Calc raw
    *((int16_t*)&temp_) = ((uint8_t*)&temp_)[0]  | ((uint8_t*)&temp_)[1]<< 8;

    //Calc temp
    *temp = (float)temp_ / 100.0;

    return ESP_OK;
}

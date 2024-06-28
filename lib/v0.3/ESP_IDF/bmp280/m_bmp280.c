#include "m_bmp280.h"
#include <inttypes.h>
#include <esp_log.h>
//#include <esp_idf_lib_helpers.h>

static const char *TAG = "BMP280_lib";

//BMP280 registers
#define BMP280_REG_TEMP_XLSB   0xFC /* bits: 7-4 */
#define BMP280_REG_TEMP_LSB    0xFB
#define BMP280_REG_TEMP_MSB    0xFA
#define BMP280_REG_TEMP        (BMP280_REG_TEMP_MSB)
#define BMP280_REG_PRESS_XLSB  0xF9 /* bits: 7-4 */
#define BMP280_REG_PRESS_LSB   0xF8
#define BMP280_REG_PRESS_MSB   0xF7
#define BMP280_REG_PRESSURE    (BMP280_REG_PRESS_MSB)
#define BMP280_REG_CONFIG      0xF5 /* bits: 7-5 t_sb; 4-2 filter; 0 spi3w_en */
#define BMP280_REG_CTRL        0xF4 /* bits: 7-5 osrs_t; 4-2 osrs_p; 1-0 mode */
#define BMP280_REG_STATUS      0xF3 /* bits: 3 measuring; 0 im_update */
#define BMP280_REG_CTRL_HUM    0xF2 /* bits: 2-0 osrs_h; */
#define BMP280_REG_RESET       0xE0
#define BMP280_REG_ID          0xD0
#define BMP280_REG_CALIB       0x88
#define BMP280_REG_HUM_CALIB   0x88

#define BMP280_RESET_VALUE     0xB6


uint8_t bmp280_read_register8(bmp280_t* dev, uint8_t reg_addr)
{
    uint8_t value;
    esp_err_t ret =  i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, &value, 1, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Read8 error!");
        #endif
        return 0;
    }

    return value;
}

uint16_t bmp280_read_register16(bmp280_t* dev, uint8_t reg_addr)
{
    uint8_t value[2] = {0, 0};
    esp_err_t ret = i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, value, 2, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Read16 error!");
        #endif
        return 0;
    }

    return (value[0] | (value[1] << 8));
}

void bmp280_read_register_sequence(bmp280_t* dev, uint8_t reg_addr, uint8_t* buf, uint8_t len)
{
    esp_err_t ret = i2c_master_write_read_device(0, dev->addr, &reg_addr, 1, buf, len, 1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "ReadSequence error!");
        #endif
        return;
    }
}

void bmp280_write_register8(bmp280_t* dev, uint8_t reg_addr, uint8_t data)
{
    uint8_t value[2] = {reg_addr, data};
    esp_err_t ret =  i2c_master_write_to_device(0, dev->addr, value, 2,  1000 / portTICK_PERIOD_MS);
    if(ret != ESP_OK)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Write8 error!");
        #endif
    }
}


esp_err_t read_calibration_data(bmp280_t *dev)
{
    dev->dig_T1 = (uint16_t)bmp280_read_register16(dev, 0x88);
    dev->dig_T2 = (int16_t)bmp280_read_register16(dev, 0x8a);
    dev->dig_T3 = (int16_t)bmp280_read_register16(dev, 0x8c);
    dev->dig_P1 = (uint16_t)bmp280_read_register16(dev, 0x8e);
    dev->dig_P2 = (int16_t)bmp280_read_register16(dev, 0x90);
    dev->dig_P3 = (int16_t)bmp280_read_register16(dev, 0x92);
    dev->dig_P4 = (int16_t)bmp280_read_register16(dev, 0x94);
    dev->dig_P5 = (int16_t)bmp280_read_register16(dev, 0x96);
    dev->dig_P6 = (int16_t)bmp280_read_register16(dev, 0x98);
    dev->dig_P7 = (int16_t)bmp280_read_register16(dev, 0x9a);
    dev->dig_P8 = (int16_t)bmp280_read_register16(dev, 0x9c);
    dev->dig_P9 = (int16_t)bmp280_read_register16(dev, 0x9e);

    #if BMP280_ENABLE_INFO
        ESP_LOGI(TAG, "Calibration data received");
    #endif

    return ESP_OK;
}


esp_err_t bmp280_init_default_params(bmp280_params_t *params)
{
    //Check pointers
    if(params == NULL)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid params!");
        #endif
        return ESP_FAIL;
    }

    //Set default param
    params->mode                        = BMP280_MODE_NORMAL;
    params->filter                      = BMP280_FILTER_OFF;
    params->oversampling_pressure       = BMP280_STANDARD;
    params->oversampling_temperature    = BMP280_STANDARD;
    params->oversampling_humidity       = BMP280_STANDARD;
    params->standby                     = BMP280_STANDBY_250;

    return ESP_OK;
}

esp_err_t bmp280_init(bmp280_t *dev, bmp280_params_t *params)
{
    //Check pointers
    if(dev == NULL || params == NULL)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or params!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read device id
    uint8_t data_ret8;
    data_ret8 = bmp280_read_register8(dev, BMP280_REG_ID);

    //Check device id
    if(data_ret8 != dev->id)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Chip ID not match!");
        #endif
        i2c_t_give_sem();
        return ESP_FAIL;
    }

    //Calibr data
    read_calibration_data(dev);

    //Set standby and filter
    uint8_t config = (params->standby << 5) | (params->filter << 2);

    bmp280_write_register8(dev, BMP280_REG_CONFIG, config); 

    //Initial mode for forced is sleep
    if (params->mode == BMP280_MODE_FORCED)
    {
        params->mode = BMP280_MODE_SLEEP;  
    }

    //Set oversamplings and mode
    uint8_t ctrl = (params->oversampling_temperature << 5) | (params->oversampling_pressure << 2) | (params->mode);
    bmp280_write_register8(dev, BMP280_REG_CTRL, ctrl); 

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bmp280_force_measurement(bmp280_t *dev)
{
    //Check pointers
    if(dev == NULL)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or params!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Get ctrl reg
    uint8_t ctrl;
    ctrl = bmp280_read_register8(dev, BMP280_REG_CTRL);

    //Setup force mode
    ctrl &= ~0b11;  // clear two lower bits
    ctrl |= BMP280_MODE_FORCED;

    //Set ctrl reg
    bmp280_write_register8(dev, BMP280_REG_CTRL, ctrl);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bmp280_is_measuring(bmp280_t *dev, bool *busy)
{
    //Check pointers
    if(dev == NULL || busy == NULL)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or busy!");
        #endif
        return ESP_FAIL;
    }

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Read status and ctrl reg
    uint8_t status_ = 0;
    uint8_t ctrl_ = 0;
    status_ = bmp280_read_register8(dev, BMP280_REG_STATUS);
    ctrl_ = bmp280_read_register8(dev, BMP280_REG_CTRL);

    // if mode FORCED => BM280 is busy 
    // if mode SLEEP => data is ready
    *busy = ((ctrl_ & 0b11) == BMP280_MODE_FORCED) || (status_ & (1 << 3));

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }
    return ESP_OK;
}

//Compensation algorithm from BMP280 datasheet
static inline int32_t compensate_temperature(bmp280_t *dev, int32_t adc_temp, int32_t *fine_temp)
{
    int32_t var1, var2;

    var1 = ((((adc_temp >> 3) - ((int32_t)dev->dig_T1 << 1))) * (int32_t)dev->dig_T2) >> 11;
    var2 = (((((adc_temp >> 4) - (int32_t)dev->dig_T1) * ((adc_temp >> 4) - (int32_t)dev->dig_T1)) >> 12) * (int32_t)dev->dig_T3) >> 14;

    *fine_temp = var1 + var2;
    return (*fine_temp * 5 + 128) >> 8;
}

//Compensation algorithm from BMP280 datasheet
static inline uint32_t compensate_pressure(bmp280_t *dev, int32_t adc_press, int32_t fine_temp)
{
    int64_t var1, var2, p;

    var1 = (int64_t)fine_temp - 128000;
    var2 = var1 * var1 * (int64_t)dev->dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->dig_P5) << 17);
    var2 = var2 + (((int64_t)dev->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->dig_P3) >> 8) + ((var1 * (int64_t)dev->dig_P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * ((int64_t)dev->dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0;  // avoid exception caused by division by zero
    }

    p = 1048576 - adc_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)dev->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)dev->dig_P8 * p) >> 19;

    p = ((p + var1 + var2) >> 8) + ((int64_t)dev->dig_P7 << 4);
    return p;
}

esp_err_t bmp280_read_fixed(bmp280_t *dev, int32_t *temperature, uint32_t *pressure)
{
    //Check pointers
    if(dev == NULL || temperature == NULL || pressure == NULL)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "Invalid dev or temperature or pressure!");
        #endif
        return ESP_FAIL;
    }

    int32_t adc_pressure;
    int32_t adc_temp;
    uint8_t data[8];

    //Lock i2c
    if(i2c_t_take_sem(100) != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Get raw lbs
    bmp280_read_register_sequence(dev, 0xf7, data, 6);

    //Unlock i2c
    if(i2c_t_give_sem() != pdPASS)
    {
        #if BMP280_ENABLE_ERROR
            ESP_LOGE(TAG, "I2C resource error!");
        #endif
        return ESP_FAIL;
    }

    //Assemble raw data
    adc_pressure = data[0] << 12 | data[1] << 4 | data[2] >> 4;
    adc_temp = data[3] << 12 | data[4] << 4 | data[5] >> 4;

    //Calc fixed data
    int32_t fine_temp;
    *temperature = compensate_temperature(dev, adc_temp, &fine_temp);
    *pressure = compensate_pressure(dev, adc_pressure, fine_temp);

    return ESP_OK;
}

esp_err_t bmp280_read_float(bmp280_t *dev, float *temperature, float *pressure)
{
    int32_t fixed_temperature = 0;
    uint32_t fixed_pressure   = 0;

    //Get fixed data
    bmp280_read_fixed(dev, &fixed_temperature, &fixed_pressure);

    //Calc data
    *temperature = (float)fixed_temperature / 100.0;
    *pressure = (float)fixed_pressure / 256.0;

    return ESP_OK;
}




#include "m_mux.h"
#include "esp_log.h"

static const char *TAG = "MUX_lib";

adc_oneshot_unit_handle_t       adc_handle;
adc_oneshot_unit_init_cfg_t     init_config;
adc_oneshot_chan_cfg_t          adc_config;
adc_cali_handle_t               adc_cali;
adc_cali_curve_fitting_config_t cali_config;

uint8_t mux_adc_mode = false;

void adc_init()
{
    //Config cali
    cali_config.unit_id  = ADC_UNIT_1;
    cali_config.atten    = ADC_ATTEN_DB_11;
    cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    //Calibrate ADC
    adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali);
}


void init_mux()
{
    //Config S0 pin
    gpio_reset_pin(MUX_S0_GPIO);
    gpio_set_direction(MUX_S0_GPIO, GPIO_MODE_OUTPUT);

    //Config S1 pin
    gpio_reset_pin(MUX_S1_GPIO);
    gpio_set_direction(MUX_S1_GPIO, GPIO_MODE_OUTPUT);

    //Config S2 pin
    gpio_reset_pin(MUX_S2_GPIO);
    gpio_set_direction(MUX_S2_GPIO, GPIO_MODE_OUTPUT);

    //Config INOUT pin
    gpio_reset_pin(MUX_INOUT_GPIO);
    gpio_set_direction(MUX_INOUT_GPIO, GPIO_MODE_INPUT);

    #if MUX_ENABLE_INFO
        ESP_LOGI(TAG, "Mux pin configured successfull!");
    #endif
}

void mux_select_line(uint8_t line)
{
    if(line>7)
    {
        #if MUX_ENABLE_ERROR
            ESP_LOGE(TAG, "Unacceptable mux line!");
        #endif
        return;
    }

    gpio_set_level(MUX_S2_GPIO, (line >> 2) & 1);
    gpio_set_level(MUX_S1_GPIO, (line >> 1) & 1);
    gpio_set_level(MUX_S0_GPIO, (line >> 0) & 1);

    #if MUX_ENABLE_INFO
        ESP_LOGI(TAG, "Change mux line to %d!", line);
    #endif
}



void mux_to_in()
{
    if(mux_adc_mode)
    {
        mux_from_analog();
    }

    gpio_reset_pin(MUX_INOUT_GPIO);
    gpio_set_direction(MUX_INOUT_GPIO, GPIO_MODE_INPUT);
    mux_adc_mode = false;
}

void mux_to_out()
{
    if(mux_adc_mode)
    {
        mux_from_analog();
    }
    gpio_reset_pin(MUX_INOUT_GPIO);
    gpio_set_direction(MUX_INOUT_GPIO, GPIO_MODE_OUTPUT);
    mux_adc_mode = false;
}

void mux_to_analog()
{
    if(mux_adc_mode)
    {
        #if MUX_ENABLE_ERROR
            ESP_LOGE(TAG, "MUX is already in analog mode!");
        #endif
        return;
    }

    gpio_reset_pin(MUX_INOUT_GPIO);

    //Config config
    init_config.unit_id = ADC_UNIT_1;
    adc_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_config.atten    = ADC_ATTEN_DB_11;

    //Create oneshot unit
    adc_oneshot_new_unit(&init_config, &adc_handle);
    adc_oneshot_config_channel(adc_handle, MUX_INOUT_ADC_CHANNEL, &adc_config);

    mux_adc_mode = true;
}

void mux_from_analog()
{
    if(!mux_adc_mode)
    {
        #if MUX_ENABLE_ERROR
            ESP_LOGE(TAG, "MUX NOT in analog mode!");
        #endif
        return;
    }
   adc_oneshot_del_unit(adc_handle);
}




uint8_t mux_get_digital()
{
    return (uint8_t)gpio_get_level(MUX_INOUT_GPIO);
}

void mux_set_digital(uint8_t val)
{
    gpio_set_level(MUX_INOUT_GPIO, val);
}

int mux_get_analog()
{
    int raw_bit  = 0;
    int raw_volt = 0;

    adc_oneshot_read(adc_handle, MUX_INOUT_ADC_CHANNEL, &raw_bit);
    adc_cali_raw_to_voltage(adc_cali, raw_bit, &raw_volt);

    return raw_volt;
}
#include "m_ra01s.h"
#include "m_spi_thread.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <ctype.h>
#include <math.h>

static const char *TAG = "ra01s_lib";

static sdmmc_host_t* host;
static spi_device_interface_config_t devcfg;
static spi_device_handle_t spi;

ra01s_locals_t ra01s_locals = {0};
static bool ra01s_inited = false;

static bool ra01s_Begin(int freq, int power);
static bool ra01s_Config(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, uint16_t preambleLength, uint8_t payloadLen, bool crcOn, bool invertIrq);

static void ra01s_reset(void);
static void ra01s_waitForIdle();
static void ra01s_setStandby(uint8_t mode);
static void ra01s_setDio2AsRfSwitchCtrl(uint8_t enable);
//static void ra01s_setDio3AsTcxoCtrl(float voltage, uint32_t delay);
static void ra01s_calibrate(uint8_t calibParam);
static void ra01s_setRegulatorMode(uint8_t mode);
static void ra01s_setBufferBaseAddress(uint8_t txBaseAddress, uint8_t rxBaseAddress);
static void ra01s_setPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut);
static void ra01s_setOvercurrentProtection(float currentLimit);
static void ra01s_setPowerConfig(int8_t power, uint8_t rampTime);
static void ra01s_setRfFrequency(uint32_t frequency);
static void ra01s_calibrateImage(uint32_t frequency);
static void ra01s_setStopRxTimerOnPreambleDetect(bool enable);
static void ra01s_setLoRaSymbNumTimeout(uint8_t SymbNum);
static void ra01s_setPacketType(uint8_t packetType);
static void ra01s_setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask);
static void ra01s_setRx(uint32_t timeout);
static void ra01s_setTxEnable(void);
static void ra01s_setRxEnable(void);
static uint8_t ra01s_getStatus(void);
static void ra01s_fixInvertedIQ(uint8_t iqConfig);
static void ra01s_setModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, uint8_t lowDataRateOptimize);
static void ra01s_setTxPower(int8_t txPowerInDbm);
static bool ra01s_send(uint8_t *pData, uint8_t len, uint8_t mode);
static void ra01s_clearIrqStatus(uint16_t irq);
static uint16_t ra01s_getIrqStatus(void);
static void ra01s_setTx(uint32_t timeoutInMs);
static bool ra01s_available();
static uint8_t ra01s_receive(uint8_t *pData, uint16_t len);
static void ra01s_getRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer);


//HAL ZONE
static void ra01s_readRegister(uint16_t reg, uint8_t* data, uint8_t numBytes) 
{
    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[4];
    uint8_t rx_buf[4];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = SX126X_CMD_READ_REGISTER; // 0x1D
    tx_buf[1] = (reg & 0xFF00) >> 8;
    tx_buf[2] = reg & 0xff;
    tx_buf[3] = SX126X_CMD_NOP;

    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(rx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S ra01s_readRegister_cmd error!");
        #endif
    }

    //Recievie data
    tx_buf[0] = SX126X_CMD_NOP;

    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    for(uint8_t n = 0; n < numBytes; n++) {
        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S ra01s_readRegister_data error!");
            #endif
        }
        data[n] = rx_buf[0];
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();
}

static void ra01s_writeRegister(uint16_t reg, uint8_t* data, uint8_t numBytes) 
{
    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[3];
    uint8_t rx_buf[3];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = SX126X_CMD_WRITE_REGISTER; // 0x0D
    tx_buf[1] = (reg & 0xFF00) >> 8;
    tx_buf[2] = reg & 0xff;

    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(rx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S ra01s_readRegister_cmd error!");
        #endif
    }

    //Transmit data
    t.length = 8;
    t.rxlength = 8;
    t.rx_buffer = rx_buf;
    for(uint8_t n = 0; n < numBytes; n++) 
    {
        t.tx_buffer = &data[n];

        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S ra01s_readRegister_data error!");
            #endif
        }
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();
}


static void ra01s_readCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes) {
    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[1];
    uint8_t rx_buf[1];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = cmd;
    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(rx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S readCommand_cmd error!");
        #endif
    }

    //Recievie data
    tx_buf[0] = SX126X_CMD_NOP;

    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    for(uint8_t n = 0; n < numBytes; n++) {
        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S readCommand_data error!");
            #endif
        }
        data[n] = rx_buf[0];
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();
}


void ra01s_writeCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes) 
{
    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[1];
    uint8_t rx_buf[1];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = cmd;
    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(rx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S writeCommand_cmd error!");
        #endif
    }

    //Variable to save error during SPI transfer
    uint8_t status = 0;

    //Transmit data
    uint8_t in;
    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = &in;
    for(uint8_t n = 0; n < numBytes; n++) 
    {   
        tx_buf[0] = data[n];
        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S writeCommand_data error!");
            #endif
        }

        //Check status
        if(((in & 0b00001110) == SX126X_STATUS_CMD_TIMEOUT) ||
            ((in & 0b00001110) == SX126X_STATUS_CMD_INVALID) ||
            ((in & 0b00001110) == SX126X_STATUS_CMD_FAILED)) 
        {
            status = in & 0b00001110;
            break;
        } else if(in == 0x00 || in == 0xFF) 
        {
            status = SX126X_STATUS_SPI_FAILED;
            break;
        }
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();

    if (status != 0) {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S writeCommand_status error!");
        #endif
    }

    return;
}


void ra01s_writeBuffer(uint8_t *txData, uint8_t txDataLen)
{
    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = SX126X_CMD_WRITE_BUFFER;
    tx_buf[1] = 0;

    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(rx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S writeCommand_cmd error!");
        #endif
    }

    //Transmit data
    t.length = 8;
    t.rxlength = 8;
    for(uint8_t n = 0; n < txDataLen; n++) 
    {   
        tx_buf[0] = txData[n];
        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S writeBuffer_data error!");
            #endif
        }
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();
}

uint8_t ra01s_readBuffer(uint8_t *rxData, uint8_t maxLen)
{
    uint8_t offset = 0;
    uint8_t payloadLength = 0;
    ra01s_getRxBufferStatus(&payloadLength, &offset);
    if( payloadLength > maxLen )
    {
        return 0;
    }

    //Ensure BUSY is low (state meachine ready)
    ra01s_waitForIdle();

    //Prepare
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    //Send CMD
    uint8_t tx_buf[3];
    uint8_t rx_buf[3];
    gpio_set_level(RADIO_PIN_NUM_CS, 0);

    tx_buf[0] = SX126X_CMD_READ_BUFFER;
    tx_buf[1] = offset;
    tx_buf[2] = SX126X_CMD_NOP;

    t.length = 8*sizeof(tx_buf);
    t.rxlength = 8*sizeof(tx_buf);
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    if(spi_device_transmit(spi, &t) != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S writeCommand_cmd error!");
        #endif
    }

    //Recievie data
    tx_buf[0] = SX126X_CMD_NOP;

    t.length = 8;
    t.rxlength = 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    for(uint8_t n = 0; n < payloadLength; n++) {
        if(spi_device_transmit(spi, &t) != ESP_OK)
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S readCommand_data error!");
            #endif
        }
        rxData[n] = rx_buf[0];
    }

    //Stop transfer
    gpio_set_level(RADIO_PIN_NUM_CS, 1);

    //Wait for BUSY to go low
    ra01s_waitForIdle();

    return payloadLength;
}

//HIGH ZONE
esp_err_t ra01s_init(sdmmc_host_t* host_)
{   
    host = host_;

    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz=1*1000*1000;            //Clock out at 1 MHz
    devcfg.mode=0;                                //SPI mode 0
    devcfg.spics_io_num=-1;                       //CS pin
    devcfg.queue_size=7;                          //7 transactions at a time
    devcfg.pre_cb=NULL;
    devcfg.flags=SPI_DEVICE_NO_DUMMY;

    //Attach the RA01S to the SPI bus
    esp_err_t ret=spi_bus_add_device(host->slot, &devcfg, &spi);

    if(ret != ESP_OK)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S initialization error!");
        #endif
        return ESP_FAIL;
    }

    //Init cs pin
    gpio_config_t cfg = {
    .pin_bit_mask = BIT64(RADIO_PIN_NUM_CS),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = false,
    .pull_down_en = false,
    .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(RADIO_PIN_NUM_RESET, 1);

    //Init nreset pon
    cfg.pin_bit_mask = BIT64(RADIO_PIN_NUM_RESET);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = false;
    cfg.pull_down_en = false;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(RADIO_PIN_NUM_RESET, 1);

    //Init busy pin
    cfg.pin_bit_mask = BIT64(RADIO_PIN_NUM_BUSY),
    cfg.mode = GPIO_MODE_INPUT,
    cfg.pull_up_en = false,
    cfg.pull_down_en = false,
    cfg.intr_type = GPIO_INTR_DISABLE,
    gpio_config(&cfg);

    //Reset locals
    memset(&ra01s_locals.PacketParams, 0, sizeof(ra01s_locals.PacketParams));
    ra01s_locals.packet = 0;
    ra01s_locals.txActive = false;

    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    //Begin
    if(ra01s_locals.freq < 430E6 || ra01s_locals.freq>460E6) 
        ra01s_locals.freq= 435E6;

    bool ret_b = ra01s_Begin(ra01s_locals.freq , 5);
    if(ret_b == false)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S begin error!");
        #endif
        spi_t_give_sem();
        return ESP_FAIL;
    }

    //Config
    ret_b = ra01s_Config(8, 5, 4, 6, 0, true, false);
    if(ret_b == false)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S config error!");
        #endif
        spi_t_give_sem();
        return ESP_FAIL;
    }

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    #if RADIO_ENABLE_INFO
        ESP_LOGI(TAG, "RA01S inited!");
    #endif

    ra01s_inited = true;

    return ESP_OK;
}

esp_err_t ra01s_deinit()
{   
    //Delete spi device
    if(ra01s_inited)
    {
        spi_bus_remove_device(spi);
        ra01s_inited = false;
    }
    return ESP_OK;
}

esp_err_t ra01s_setLowPower()
{
    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    ra01s_setTxPower(3);

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ra01s_setMaxPower()
{
    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    ra01s_setTxPower(20);

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }
    return ESP_OK;
}


esp_err_t ra01s_setChannel(uint8_t ch_)
{
    int freq_=435E6;
    int sw_=0x13;

    switch (ch_)
    {
        case 1:{freq_=435E6;sw_=0x13;}
            break;
        case 2:{freq_=436E6;sw_=0x14;}
            break;
        case 3:{freq_=437E6;sw_=0x15;}
            break;
        case 4:{freq_=438E6;sw_=0x16;}
            break;
        case 5:{freq_=439E6;sw_=0x17;}
            break;
        case 6:{freq_=440E6;sw_=0x18;}
            break;
        default:
        {
            #if RADIO_ENABLE_ERROR
                ESP_LOGE(TAG, "RA01S channel must be 1<=ch<=6!");
            #endif
            return ESP_FAIL;
        }break;
    }

    ra01s_locals.sw = sw_;
    ra01s_locals.freq = freq_;

    ra01s_deinit();
    ra01s_init(host);

    return ESP_OK;
}

esp_err_t ra01s_sendS(char* msg_, uint8_t len)
{
    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    ra01s_send((uint8_t*)msg_, len, SX126x_TXMODE_SYNC);

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ra01s_sendTelemetryPack(bool flag_0, bool flag_1,bool flag_2, float height_, float speed_,
                                                    float Lon_,    float Lat_)
{

    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    ra01s_locals.packet++;
    uint8_t Len_=23;
    uint8_t msg_[Len_];

    uint8_t flag_=0;
    if(flag_0)flag_=1;
    if(flag_1)flag_+=2;
    if(flag_2)flag_+=4;

    int32_t Lon= (int32_t)(Lon_*1000000.0);
    int32_t Lat= (int32_t)(Lat_*1000000.0);

    msg_[0]='*';
    memcpy(msg_+1,&ra01s_locals.packet,4);
    memcpy(msg_+5,&height_,4);
    memcpy(msg_+9,&speed_,4);
    memcpy(msg_+13,&Lon,4);
    memcpy(msg_+17,&Lat,4);
    memcpy(msg_+21,&flag_,1);
    msg_[22]='#';

    ra01s_send((uint8_t*)msg_, Len_, SX126x_TXMODE_SYNC);

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ra01s_availablePacket(bool* avaliable_)
{
    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    *avaliable_ = ra01s_available();

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}
esp_err_t ra01s_reciveS(char* msg, uint8_t max_len)
{
    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    uint8_t rxLen = ra01s_receive((uint8_t*)msg, max_len);

    if(rxLen>max_len)rxLen = max_len;
    msg[rxLen] = 0;

    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ra01s_parceTelemetryPack(char* msg, uint8_t max_len)
{

    if(max_len<23)
        return ESP_FAIL;

    //Lock spi
    if(spi_t_take_sem(100) != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }

    uint8_t len_=23;
    uint8_t rxData[255];
    
    uint32_t Buff;
    float Buff_d;
    uint32_t buf_packet=0;
    uint32_t buf_lat=0;
    uint32_t buf_lon=0;
    uint32_t buf_flag=0;
    float buf_alt=0.0;
    float buf_speed=0.0;

    uint8_t rxLen = ra01s_receive((uint8_t*)rxData, max_len);
    if(rxLen>max_len)rxLen = max_len;

    esp_err_t ret = ESP_FAIL;
    if(rxLen==len_)
    {   
        ra01s_locals.packet++;

        if(rxData[0]=='*' && rxData[22]=='#')
        {
            memcpy(&Buff,&rxData[1],4); //packet
            buf_packet = Buff;

            memcpy(&Buff_d,&rxData[5],4); //height
            buf_alt = Buff_d;

            memcpy(&Buff_d,&rxData[9],4); //speed
            buf_speed = Buff_d;

            memcpy(&Buff,&rxData[13],4); //Lon
            buf_lon = Buff;
            
            memcpy(&Buff,&rxData[17],4); //Lat
            buf_lat = Buff;

            buf_flag=rxData[21]; //flag

            snprintf(msg, max_len, "R:%ld/%ld:%.2f:%.2f:%.6f:%.6f:%d:%d:%d\n" , 
                        buf_packet, ra01s_locals.packet, buf_alt, buf_speed, 
                        ((float)buf_lon/1000000.0), ((float)buf_lat/1000000.0),
                        (uint8_t)(buf_flag&0b00000001), (uint8_t)((buf_flag&0b00000010)>>1), (uint8_t)((buf_flag&0b00000100)>>2));
            ret= ESP_OK;
        }
        ret= ESP_FAIL;
    }
    else 
    {
        memcpy(msg, rxData, rxLen);
        msg[rxLen] = 0;
    }


    //Unclock spi
    if(spi_t_give_sem() != pdPASS)
    {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "SPI resource error!");
        #endif
        return ESP_FAIL;
    }


    ret= ESP_OK;   
    return ret;
}

//LOW ZONE
static bool ra01s_Begin(int freq, int power)
{
    if ( power > 22 )
        power = 22;
    if ( power < -3 )
        power = -3;
    
    ra01s_reset();

    uint8_t wk[2];
    ra01s_readRegister(SX126X_REG_LORA_SYNC_WORD_MSB, wk, 2); // 0x0740
    uint16_t syncWord = (wk[0] << 8) + wk[1];

    if (syncWord != SX126X_SYNC_WORD_PUBLIC && syncWord != SX126X_SYNC_WORD_PRIVATE) {
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S error, maybe no SPI connection!");
        #endif
        return false;
    }

    ra01s_setStandby(SX126X_STANDBY_RC);
    ra01s_setDio2AsRfSwitchCtrl(true);

    //Set TCXO control, if requested
    //Configure the RA01S to use a TCXO controlled by DIO3
    //ra01s_setDio3AsTcxoCtrl(tcxoVoltage, RADIO_TCXO_SETUP_TIME); 

    ra01s_calibrate( SX126X_CALIBRATE_IMAGE_ON
                    | SX126X_CALIBRATE_ADC_BULK_P_ON
                    | SX126X_CALIBRATE_ADC_BULK_N_ON
                    | SX126X_CALIBRATE_ADC_PULSE_ON
                    | SX126X_CALIBRATE_PLL_ON
                    | SX126X_CALIBRATE_RC13M_ON
                    | SX126X_CALIBRATE_RC64K_ON);

    //ra01s_setRegulatorMode(SX126X_REGULATOR_LDO); //Set regulator mode: LDO
    ra01s_setRegulatorMode(SX126X_REGULATOR_DC_DC); //Set regulator mode: DC-DC

    ra01s_setBufferBaseAddress(0, 0);

    // ra01s_setPaConfig(0x06, 0x00, 0x01, 0x01); //PA Optimal Settings +15 dBm
    ra01s_setPaConfig(0x04, 0x07, 0x00, 0x01); //PA Optimal Settings +22 dBm

    //Current max 120mA for the whole device
    ra01s_setOvercurrentProtection(120.0);  

    //0 fuer Empfaenger
    ra01s_setPowerConfig(power, SX126X_PA_RAMP_200U); 
    ra01s_setRfFrequency(freq);

    return true;
}


static bool ra01s_Config(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, uint16_t preambleLength, uint8_t payloadLen, bool crcOn, bool invertIrq)
{
    ra01s_setStopRxTimerOnPreambleDetect(false);
    ra01s_setLoRaSymbNumTimeout(0); 

    //SX126x.ModulationParams.PacketType : MODEM_LORA
    ra01s_setPacketType(SX126X_PACKET_TYPE_LORA);      

    //LowDataRateOptimize OFF
    uint8_t ldro = 0; 
    ra01s_setModulationParams(spreadingFactor, bandwidth, codingRate, ldro);

    ra01s_locals.PacketParams[0] = (preambleLength >> 8) & 0xFF;
    ra01s_locals.PacketParams[1] = preambleLength;
    if (payloadLen)
    {
        ra01s_locals.PacketParams[2] = 0x01; // Fixed length packet (implicit header)
        ra01s_locals.PacketParams[3] = payloadLen;
    }
    else
    {
        ra01s_locals.PacketParams[2] = 0x00; // Variable length packet (explicit header)
        ra01s_locals.PacketParams[3] = 0xFF;
    }

    if (crcOn)
        ra01s_locals.PacketParams[4] = SX126X_LORA_IQ_INVERTED;
    else
        ra01s_locals.PacketParams[4] = SX126X_LORA_IQ_STANDARD;

    if (invertIrq)
        ra01s_locals.PacketParams[5] = 0x01; //Inverted LoRa I and Q signals setup
    else
        ra01s_locals.PacketParams[5] = 0x00; //Standard LoRa I and Q signals setup

    // fixes IQ configuration for inverted IQ
    ra01s_fixInvertedIQ(ra01s_locals.PacketParams[5]);

    ra01s_writeCommand(SX126X_CMD_SET_PACKET_PARAMS, ra01s_locals.PacketParams, 6); // 0x8C


    // ra01s_setDioIrqParams(SX126X_IRQ_ALL,  //all interrupts enabled
    //                 (SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT), //interrupts on DIO1
    //                 SX126X_IRQ_NONE,  //interrupts on DIO2
    //                 SX126X_IRQ_NONE); //interrupts on DIO3

    // Do not use DIO interruptst
    ra01s_setDioIrqParams(SX126X_IRQ_ALL,   //all interrupts enabled
                    SX126X_IRQ_NONE,  //interrupts on DIO1
                    SX126X_IRQ_NONE,  //interrupts on DIO2
                    SX126X_IRQ_NONE); //interrupts on DIO3

    // Receive state no receive timeoout
    ra01s_setRx(0xFFFFFF);
    return true;
}

static bool ra01s_send(uint8_t *pData, uint8_t len, uint8_t mode)
{
  uint16_t irqStatus;
  bool rv = false;
  
  if (ra01s_locals.txActive == false )
  {
    ra01s_locals.txActive = true;
    
    ra01s_locals.PacketParams[2] = 0x00; //Variable length packet (explicit header)
    ra01s_locals.PacketParams[3] = len;
    ra01s_writeCommand(SX126X_CMD_SET_PACKET_PARAMS, ra01s_locals.PacketParams, 6); // 0x8C

    ra01s_clearIrqStatus(SX126X_IRQ_ALL);

    ra01s_writeBuffer(pData, len);
    ra01s_setTx(500);

    if ( mode & SX126x_TXMODE_SYNC )
    {
      irqStatus = ra01s_getIrqStatus();
      while ( (!(irqStatus & SX126X_IRQ_TX_DONE)) && (!(irqStatus & SX126X_IRQ_TIMEOUT)) )
      {
        vTaskDelay(1);
        irqStatus = ra01s_getIrqStatus();
      }

      ra01s_locals.txActive = false;

      ra01s_setRx(0xFFFFFF);

      if ( irqStatus & SX126X_IRQ_TX_DONE) {
        rv = true;
      }
    }
    else
    {
      rv = true;
    }
  }

  return rv;
}

static uint8_t ra01s_receive(uint8_t *pData, uint16_t len) 
{
  uint8_t rxLen = 0;
  uint16_t irqRegs = ra01s_getIrqStatus();
  //uint8_t status = ra01s_getStatus();

  if( irqRegs & SX126X_IRQ_RX_DONE )
  {
    ra01s_clearIrqStatus(SX126X_IRQ_ALL);
    rxLen = ra01s_readBuffer(pData, len);
  }
  return rxLen;
}

static void ra01s_reset(void)
{
  vTaskDelay(10 / portTICK_PERIOD_MS);
  gpio_set_level(RADIO_PIN_NUM_RESET,0);
  vTaskDelay(20 / portTICK_PERIOD_MS);
  gpio_set_level(RADIO_PIN_NUM_RESET,1);
  vTaskDelay(10 / portTICK_PERIOD_MS);
  // ensure BUSY is low (state meachine ready)
  ra01s_waitForIdle();
}


static void ra01s_waitForIdle()
{
    uint64_t start = esp_timer_get_time();
    for(uint64_t start = esp_timer_get_time(); esp_timer_get_time()<start+1000 ;) {taskYIELD();}   //~1ms delay
    while(gpio_get_level(RADIO_PIN_NUM_BUSY)) {
        if(esp_timer_get_time() - start >= 1000000) {   //1 sec timeout
            break;
    }
    vTaskDelay(1);
    }
}

static void ra01s_setStandby(uint8_t mode)
{
  uint8_t data = mode;
  ra01s_writeCommand(SX126X_CMD_SET_STANDBY, &data, 1); // 0x80
}

static void ra01s_setDio2AsRfSwitchCtrl(uint8_t enable)
{
  uint8_t data = enable;
  ra01s_writeCommand(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &data, 1); // 0x9D
}

static void ra01s_calibrate(uint8_t calibParam)
{
  uint8_t data = calibParam;
  ra01s_writeCommand(SX126X_CMD_CALIBRATE, &data, 1); // 0x89
}

static void ra01s_setRegulatorMode(uint8_t mode)
{
  uint8_t data = mode;
  ra01s_writeCommand(SX126X_CMD_SET_REGULATOR_MODE, &data, 1); // 0x96
}

static void ra01s_setBufferBaseAddress(uint8_t txBaseAddress, uint8_t rxBaseAddress)
{
  uint8_t buf[2];

  buf[0] = txBaseAddress;
  buf[1] = rxBaseAddress;
  ra01s_writeCommand(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, buf, 2); // 0x8F
}

static void ra01s_setPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut)
{
  uint8_t buf[4];

  buf[0] = paDutyCycle;
  buf[1] = hpMax;
  buf[2] = deviceSel;
  buf[3] = paLut;
  ra01s_writeCommand(SX126X_CMD_SET_PA_CONFIG, buf, 4); // 0x95
}

static void ra01s_setOvercurrentProtection(float currentLimit)
{
  if((currentLimit >= 0.0) && (currentLimit <= 140.0)) {
    uint8_t buf[1];
    buf[0] = (uint8_t)(currentLimit / 2.5);
    ra01s_writeRegister(SX126X_REG_OCP_CONFIGURATION, buf, 1); // 0x08E7
  }
}

static void ra01s_setPowerConfig(int8_t power, uint8_t rampTime)
{
  uint8_t buf[2];

  if( power > 22 )
    power = 22;
  else if( power < -3 )
    power = -3;
    
  buf[0] = power;
  buf[1] = (uint8_t)rampTime;
  ra01s_writeCommand(SX126X_CMD_SET_TX_PARAMS, buf, 2); // 0x8E
}

static void ra01s_setRfFrequency(uint32_t frequency)
{
  uint8_t buf[4];
  uint32_t freq = 0;

  ra01s_calibrateImage(frequency);

  freq = (uint32_t)((float)frequency / (float)SX1261_FREQ_STEP);
  buf[0] = (uint8_t)((freq >> 24) & 0xFF);
  buf[1] = (uint8_t)((freq >> 16) & 0xFF);
  buf[2] = (uint8_t)((freq >> 8) & 0xFF);
  buf[3] = (uint8_t)(freq & 0xFF);
  ra01s_writeCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4); // 0x86
}

static void ra01s_calibrateImage(uint32_t frequency)
{
  uint8_t calFreq[2];

  if( frequency> 900000000 )
  {
    calFreq[0] = 0xE1;
    calFreq[1] = 0xE9;
  }
  else if( frequency > 850000000 )
  {
    calFreq[0] = 0xD7;
    calFreq[1] = 0xD8;
  }
  else if( frequency > 770000000 )
  {
    calFreq[0] = 0xC1;
    calFreq[1] = 0xC5;
  }
  else if( frequency > 460000000 )
  {
    calFreq[0] = 0x75;
    calFreq[1] = 0x81;
  }
  else if( frequency > 425000000 )
  {
    calFreq[0] = 0x6B;
    calFreq[1] = 0x6F;
  }
  ra01s_writeCommand(SX126X_CMD_CALIBRATE_IMAGE, calFreq, 2); // 0x98
}

static void ra01s_setStopRxTimerOnPreambleDetect(bool enable)
{
  uint8_t data = (uint8_t)enable;
  ra01s_writeCommand(SX126X_CMD_STOP_TIMER_ON_PREAMBLE, &data, 1); // 0x9F
}

static void ra01s_setLoRaSymbNumTimeout(uint8_t SymbNum)
{
  uint8_t data = SymbNum;
  ra01s_writeCommand(SX126X_CMD_SET_LORA_SYMB_NUM_TIMEOUT, &data, 1); // 0xA0
}

static void ra01s_setPacketType(uint8_t packetType)
{
  uint8_t data = packetType;
  ra01s_writeCommand(SX126X_CMD_SET_PACKET_TYPE, &data, 1); // 0x01
}

static void ra01s_setModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, uint8_t lowDataRateOptimize)
{
  uint8_t data[4];
  //currently only LoRa supported
  data[0] = spreadingFactor;
  data[1] = bandwidth;
  data[2] = codingRate;
  data[3] = lowDataRateOptimize;
  ra01s_writeCommand(SX126X_CMD_SET_MODULATION_PARAMS, data, 4); // 0x8B
}

static void ra01s_setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
{
  uint8_t buf[8];

  buf[0] = (uint8_t)((irqMask >> 8) & 0x00FF);
  buf[1] = (uint8_t)(irqMask & 0x00FF);
  buf[2] = (uint8_t)((dio1Mask >> 8) & 0x00FF);
  buf[3] = (uint8_t)(dio1Mask & 0x00FF);
  buf[4] = (uint8_t)((dio2Mask >> 8) & 0x00FF);
  buf[5] = (uint8_t)(dio2Mask & 0x00FF);
  buf[6] = (uint8_t)((dio3Mask >> 8) & 0x00FF);
  buf[7] = (uint8_t)(dio3Mask & 0x00FF);
  ra01s_writeCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, buf, 8); // 0x08
}

static void ra01s_setRx(uint32_t timeout)
{
  ra01s_setStandby(SX126X_STANDBY_RC);
  ra01s_setRxEnable();

  uint8_t buf[3];
  buf[0] = (uint8_t)((timeout >> 16) & 0xFF);
  buf[1] = (uint8_t)((timeout >> 8) & 0xFF);
  buf[2] = (uint8_t )(timeout & 0xFF);
  ra01s_writeCommand(SX126X_CMD_SET_RX, buf, 3); // 0x82

  for(int retry=0;retry<10;retry++) 
  {
    if ((ra01s_getStatus() & 0x70) == 0x50) break;

    for(uint64_t start = esp_timer_get_time(); esp_timer_get_time()<start+1000 ;) {taskYIELD();}   //~1ms delay
  }

  if ((ra01s_getStatus() & 0x70) != 0x50) 
  {
    //Serial.println("SetRx Illegal Status");
        #if RADIO_ENABLE_ERROR
            ESP_LOGE(TAG, "RA01S SetRx Illegal Status!");
        #endif
  }
}

static void ra01s_setRxEnable(void){;}
static void ra01s_setTxEnable(void){;}

static uint8_t ra01s_getStatus(void)
{
    uint8_t rv;
    ra01s_readCommand(SX126X_CMD_GET_STATUS, &rv, 1); // 0xC0
    return rv;
}

static void ra01s_fixInvertedIQ(uint8_t iqConfig)
{
    // fixes IQ configuration for inverted IQ
    // see SX1262/SX1268 datasheet, chapter 15 Known Limitations, section 15.4 for details
    // When exchanging LoRa packets with inverted IQ polarity, some packet losses may be observed for longer packets.
    // Workaround: Bit 2 at address 0x0736 must be set to:
    // 0 when using inverted IQ polarity (see the SetPacketParam(...) command)
    // 1 when using standard IQ polarity

    // read current IQ configuration
    uint8_t iqConfigCurrent = 0;
    ra01s_readRegister(SX126X_REG_IQ_POLARITY_SETUP, &iqConfigCurrent, 1); // 0x0736

    // set correct IQ configuration
    if(iqConfig == SX126X_LORA_IQ_STANDARD) {
    iqConfigCurrent &= 0xFB;
    } else {
    iqConfigCurrent |= 0x04;
    }
      
    // update with the new value
    ra01s_writeRegister(SX126X_REG_IQ_POLARITY_SETUP, &iqConfigCurrent, 1); // 0x0736
}

static void ra01s_setTxPower(int8_t txPowerInDbm)
{
    ra01s_setPowerConfig(txPowerInDbm, SX126X_PA_RAMP_200U);
}

static void ra01s_clearIrqStatus(uint16_t irq)
{
    uint8_t buf[2];

    buf[0] = (uint8_t)(((uint16_t)irq >> 8) & 0x00FF);
    buf[1] = (uint8_t)((uint16_t)irq & 0x00FF);
    ra01s_writeCommand(SX126X_CMD_CLEAR_IRQ_STATUS, buf, 2); // 0x02
}

static uint16_t ra01s_getIrqStatus(void)
{
    uint8_t data[3];
    ra01s_readCommand(SX126X_CMD_GET_IRQ_STATUS, data, 3); // 0x12

    // uint8_t buf[4] = {0};
    // ra01s_readCommand( SX126X_CMD_GET_DEVICE_ERRORS, buf, 4 ); // 0x14)
    // ESP_LOGE(TAG, "buf = %d, %d, %d , %d", buf[0], buf[1], buf[2], buf[3]);

    // ESP_LOGE(TAG, "%d, %d, %d", data[0], data[1], data[2]);
    return (data[1] << 8) | data[2];
}

static void ra01s_setTx(uint32_t timeoutInMs)
{

  ra01s_setStandby(SX126X_STANDBY_RC);
  ra01s_setTxEnable();
  
  uint8_t buf[3];
  uint32_t tout = timeoutInMs;
  if (timeoutInMs != 0) {
    // Timeout duration = Timeout * 15.625 ��s
    uint32_t timeoutInUs = timeoutInMs * 1000;
    tout = (uint32_t)(timeoutInUs / 15.625);
  }

  buf[0] = (uint8_t)((tout >> 16) & 0xFF);
  buf[1] = (uint8_t)((tout >> 8) & 0xFF);
  buf[2] = (uint8_t )(tout & 0xFF);
  ra01s_writeCommand(SX126X_CMD_SET_TX, buf, 3); // 0x83

  for(int retry=0;retry<10;retry++) {
    if ((ra01s_getStatus() & 0x70) == 0x60) break;
    
    for(uint64_t start = esp_timer_get_time(); esp_timer_get_time()<start+1000 ;) {taskYIELD();}   //~1ms delay
  }
  if ((ra01s_getStatus() & 0x70) != 0x60) 
  {
    #if RADIO_ENABLE_ERROR
        ESP_LOGE(TAG, "RA01S SetTx Illegal Status!");
    #endif
  }
}

static bool ra01s_available() 
{
  uint16_t irqRegs = ra01s_getIrqStatus();
  return ( (irqRegs & SX126X_IRQ_RX_DONE) != 0);
}

static void ra01s_getRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer)
{
  uint8_t buf[3];
  ra01s_readCommand( SX126X_CMD_GET_RX_BUFFER_STATUS, buf, 3 ); // 0x13
  *payloadLength = buf[1];
  *rxStartBufferPointer = buf[2];
}


//Not used
// static void ra01s_setDio3AsTcxoCtrl(float voltage, uint32_t delay)
// {
//   uint8_t buf[4];

//   //buf[0] = tcxoVoltage & 0x07;
//   if(fabs(voltage - 1.6) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_1_6;
//   } else if(fabs(voltage - 1.7) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_1_7;
//   } else if(fabs(voltage - 1.8) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_1_8;
//   } else if(fabs(voltage - 2.2) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_2_2;
//   } else if(fabs(voltage - 2.4) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_2_4;
//   } else if(fabs(voltage - 2.7) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_2_7;
//   } else if(fabs(voltage - 3.0) <= 0.001) {
//     buf[0] = SX126X_DIO3_OUTPUT_3_0;
//   } else {
//     buf[0] = SX126X_DIO3_OUTPUT_3_3;
//   }

//   uint32_t delayValue = (float)delay / 15.625;
//   buf[1] = ( uint8_t )( ( delayValue >> 16 ) & 0xFF );
//   buf[2] = ( uint8_t )( ( delayValue >> 8 ) & 0xFF );
//   buf[3] = ( uint8_t )( delayValue & 0xFF );

//   ra01s_writeCommand(SX126X_CMD_SET_DIO3_AS_TCXO_CTRL, buf, 4); // 0x97
// }

// bool SX126x::ReceiveMode(void)
// {
//   uint16_t irq;
//   bool rv = false;

//   if ( ra01s_locals.txActive == false )
//   {
//     rv = true;
//   }
//   else
//   {
//     irq = ra01s_getIrqStatus();
//     if ( irq & (SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT) )
//     { 
//       SetRx(0xFFFFFF);
//       ra01s_locals.txActive = false;
//       rv = true;
//     }
//   }

//   return rv;
// }


// void SX126x::GetPacketStatus(int8_t *rssiPacket, int8_t *snrPacket)
// {
//   uint8_t buf[4];
//   ra01s_readCommand( SX126X_CMD_GET_PACKET_STATUS, buf, 4 ); // 0x14
//   *rssiPacket = (buf[3] >> 1) * -1;
//   ( buf[2] < 128 ) ? ( *snrPacket = buf[2] >> 2 ) : ( *snrPacket = ( ( buf[2] - 256 ) >> 2 ) );
// }


// void SX126x::Wakeup(void)
// {
//   ra01s_getStatus();
// }

// uint8_t SX126x::GetRssiInst()
// {
//   uint8_t buf[2];
//   ra01s_readCommand( SX126X_CMD_GET_RSSI_INST, buf, 2 ); // 0x15
//   return buf[1];
// }



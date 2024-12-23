#include "gatt_echo.h"
#include "m_ble_low.h"

static const char *TAG = "BLE_GATT_lib";

#define BLE_GATT_ECHO_LOG_DEGUG true

/****************************** GATT TABLE DEF ******************************/

#define ECHO_GATTS_CHAR_VAL_LEN_MAX  250   //Characteristic max byte lenght
#define ECHO_GATTS_CHAR_VAL_LEN_INIT 2     //Characteristic init byte lenght
#define ECHO_CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

//Auxiliary enumeration defining indices in the attribute table esp-idf
enum
{
    ECHO_INDX_SVC,               //Service index

    ECHO_INDX_CHAR_IN,           //Input characteristic index 
    ECHO_INDX_CHAR_IN_VAL,       //Input characteristic value 

    ECHO_INDX_CHAR_ECHO,         //ECHO characteristic index 
    ECHO_INDX_CHAR_ECHO_VAL,     //ECHO characteristic value 
    ECHO_INDX_CHAR_ECHO_CFG,     //ECHO characteristic configuration (Notification on/off) 

    ECHO_INDX_NB                 //Total number of indices in the table
};

//Definition to table esp-idf
static const uint16_t ECHO_GATTS_SERVICE_UUID            = 0x00FF;      //UUID - just unique identifier
static const uint16_t ECHO_GATTS_CHAR_CHAR_IN_UUID       = 0xFF01;      //Manual setting
static const uint16_t ECHO_GATTS_CHAR_CHAR_ECHO_UUIDO    = 0xFF02;

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;     //According to the BLE specification
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
// static const uint8_t char_prop_read                = ESP_GATT_CHAR_PROP_BIT_READ;
// static const uint8_t char_prop_write               = ESP_GATT_CHAR_PROP_BIT_WRITE;  // Increase speed ESP_GATT_CHAR_PROP_BIT_WRITE_NR ?
static const uint8_t char_prop_read_write          = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
static const uint8_t char_prop_read_write_notify   = char_prop_read_write | ESP_GATT_CHAR_PROP_BIT_INDICATE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static uint8_t input_value[ECHO_GATTS_CHAR_VAL_LEN_MAX] = {0x12,0x21,};  //default input value
static uint8_t echo_value[ECHO_GATTS_CHAR_VAL_LEN_MAX]  = {0x72,0x09,};  //default echo value
static uint8_t echo_ccc[2]  = {0x00, 0x00};                              //client characteristic configuration (Indication or Notification) 

//Full Database Description - Used to add attributes into the database
static esp_gatts_attr_db_t gatt_db[ECHO_INDX_NB] =
{
    //Service Declaration
    [ECHO_INDX_SVC]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(ECHO_GATTS_SERVICE_UUID), (uint8_t *)&ECHO_GATTS_SERVICE_UUID}},

    //Characteristic "Input" declaration - read/write
    [ECHO_INDX_CHAR_IN]     =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,     //Client able only read
      ECHO_CHAR_DECLARATION_SIZE, ECHO_CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},           //Server able read and write

    //Characteristic "Input" value
    [ECHO_INDX_CHAR_IN_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&ECHO_GATTS_CHAR_CHAR_IN_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      ECHO_GATTS_CHAR_VAL_LEN_MAX, sizeof(input_value), (uint8_t *)input_value}},

    //Characteristic "Echo" declaration read/write/notify
    [ECHO_INDX_CHAR_ECHO]      =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      ECHO_CHAR_DECLARATION_SIZE, ECHO_CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write_notify}},

    //Characteristic "Echo" value
    [ECHO_INDX_CHAR_ECHO_VAL]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&ECHO_GATTS_CHAR_CHAR_ECHO_UUIDO, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      ECHO_GATTS_CHAR_VAL_LEN_MAX, sizeof(echo_value), (uint8_t *)echo_value}},

    ///Characteristic "Echo" ccc descriptor - read/write
    [ECHO_INDX_CHAR_ECHO_CFG]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(echo_ccc), (uint8_t *)echo_ccc}},
};
#define ECHO_INST_ID                 0       //Table ID in bluedriod stack
uint16_t echo_handle_table[ECHO_INDX_NB];    //Array of index by attribute table ?, init in gatts callback

/****************************** GAP ADVERTISING DEF ******************************/
#define DEVICE_NAME          "ECHO_TEST_SERVER"

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

//First uuid, 16bit, [12],[13] is the value
static uint8_t service_uuid[16] = {
    //LSB <--------------------------------------------------------------------------------> MSB
    //So ECHO_GATTS_SERVICE_UUID = 0x00FF write over here ---------> (    |     )
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};
static uint8_t esp32s3_code[4] = {
    0x15, 0x72, 0xFC, 0x09
};

// !The length of adv data must be less than 31 bytes!
static esp_ble_adv_data_t adv_data = {                                      //Example lenght calc
    .set_scan_rsp        = false,   
    .include_name        = false,                                           // 2 + len(DEVICE_NAME) -> 0
    .include_txpower     = true,                                            // 2 + 1 -> 3
    .min_interval        = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval        = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance          = 0x00,
    .manufacturer_len    = sizeof(esp32s3_code),
    .p_manufacturer_data = esp32s3_code,                                    // 2 + manufacturer_len -> 6
    .service_data_len    = 0,
    .p_service_data      = NULL,                                            // 2 + service_data_len -> 0
    .service_uuid_len    = sizeof(service_uuid),                            
    .p_service_uuid      = service_uuid,                                    // 2 + service_uuid_len -> 18
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),   // 2 + 1 -> 3
};                                                                          //Total = 30 byte

//!The length of scan response data  must be less than 31 bytes!
static esp_ble_adv_data_t scan_rsp_data = {                                 //Example lenght calc
    .set_scan_rsp        = true,
    .include_name        = true,                                            // 2 + len(DEVICE_NAME) -> 18
    .include_txpower     = true,                                            // 2 + 1 -> 3
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = sizeof(esp32s3_code),
    .p_manufacturer_data = esp32s3_code,                                    // 2 + manufacturer_len -> 6
    .service_data_len    = 0,
    .p_service_data      = NULL,                                            // 2 + service_data_len -> 0
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,                                            // 2 + service_uuid_len -> 0
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),   // 2 + 1 -> 3
};                                                                          //Total = 30 byte

//Adv param
static uint8_t adv_config_done = 0;
static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20, //Minimum advertising interval for undirected and low duty cycle directed advertising, ms
    .adv_int_max         = 0x40, //Maximum advertising interval for undirected and low duty cycle directed advertising, ms
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/****************************** GATT PROFILE DEF ******************************/
//Number of instance
#define ECHO_PROFILE_NUM                 1
#define ECHO_PROFILE_APP_IDX             0
#define ECHO_APP_ID                      0x55

//Proto
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};
static void gatts_profile_event_handler(esp_gatts_cb_event_t event,esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

//One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT
static struct gatts_profile_inst echo_profile_tab[ECHO_PROFILE_NUM] = {
    [ECHO_PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       //Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/****************************** ECHO FUCNTION ******************************/

//Echo write_event
static void write_event(uint16_t index, uint8_t* data, uint16_t len, esp_ble_gatts_cb_param_t *param)
{   
    //Extract decs
    esp_attr_desc_t* decs = (esp_attr_desc_t*)&gatt_db[index].att_desc;

    //Truncate len
    if(len > ECHO_GATTS_CHAR_VAL_LEN_MAX)
        len = ECHO_GATTS_CHAR_VAL_LEN_MAX;

    if(index == ECHO_INDX_CHAR_IN_VAL)
    {
        //Check len
        if(len > decs->max_length)
            goto exit;

        //Write in input
        decs->length = len;
        memcpy(decs->value, data, len);

        //Write in echo
        esp_attr_desc_t* decs_echo = (esp_attr_desc_t*)&gatt_db[ECHO_INDX_CHAR_ECHO_VAL].att_desc;
        decs_echo->length = len;
        memcpy(decs_echo->value, data, len);

        //Update table
        esp_ble_gatts_set_attr_value(echo_handle_table[index], len, decs->value);
        esp_ble_gatts_set_attr_value(echo_handle_table[ECHO_INDX_CHAR_ECHO_VAL], len, decs_echo->value);

        //Check indication/notification
        esp_attr_desc_t* decs_cccd = (esp_attr_desc_t*)&gatt_db[ECHO_INDX_CHAR_ECHO_CFG].att_desc;
        uint8_t enable_indication = decs_cccd->value[0];
        if(enable_indication != 0x0) //Alwais indications 
        {
            esp_ble_gatts_send_indicate(echo_profile_tab[ECHO_PROFILE_APP_IDX].gatts_if, param->write.conn_id, echo_handle_table[ECHO_INDX_CHAR_ECHO_VAL],
                                len, decs_echo->value, true);
        }
    }

    else if(index == ECHO_INDX_CHAR_ECHO_VAL)
    {
        //Check len
        if(len > decs->max_length)
            goto exit;

        //Write in echo
        decs->length = len;
        memcpy(decs->value, data, len);

        //Update table
        esp_ble_gatts_set_attr_value(echo_handle_table[index], len, decs->value);
    }

    else if(index == ECHO_INDX_CHAR_ECHO_CFG)
    {
        //Only 2 byte
        if(len != decs->max_length)
            goto exit;
        
        //Write
        decs->length = len;
        memcpy(decs->value, data, len);

        //Update table
        esp_ble_gatts_set_attr_value(echo_handle_table[index], len, decs->value);
    }

exit:
    //Send response
    if (param->write.need_rsp){
        esp_ble_gatts_send_response(echo_profile_tab[ECHO_PROFILE_APP_IDX].gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
    }
} 

/****************************** INTERNAL FUCNTION ******************************/
static uint16_t mtu_size = BLE_LOW_MTU_SIZE_DEFAULT;

//Helper function
static uint8_t find_char_and_desr_index(uint16_t handle)
{
    uint8_t error = 0xff;
    for(int i = 0; i < ECHO_INDX_NB ; i++)
        if(handle == echo_handle_table[i])
            return i;
    return error;
}

//Manual control read operation - need set ESP_GATT_RSP_BY_APP
//When set ESP_GATT_AUTO_RSP - don't call this function
// !Just template, not for use!
static esp_gatt_rsp_t* read_and_read_blob(esp_gatt_status_t* ret, esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{   
    static esp_gatt_rsp_t rsp;  

    //Prepare default responce
    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));  
    rsp.attr_value.handle = param->read.handle;  
    *ret = ESP_GATT_ERROR;

    //Find index
    uint8_t indx = find_char_and_desr_index(param->read.handle);

    //Prepare responce data
    if( indx != ECHO_INDX_CHAR_IN_VAL   &&
        indx != ECHO_INDX_CHAR_ECHO_VAL &&      //all readable val and disc
        indx != ECHO_INDX_CHAR_ECHO_CFG
    )
        return &rsp;

    //Extract decs
    esp_attr_desc_t decs = gatt_db[indx].att_desc;

    //Check blob read
    if(param->read.is_long)
    {
        //Check offset
        if (param->read.offset >= decs.length)
            return &rsp;
        rsp.attr_value.offset = param->read.offset;

        //Cacl len
        uint16_t max_len = mtu_size - 1;
        uint16_t remaining_len = decs.length - param->read.offset;
        uint16_t send_len = (remaining_len > max_len) ? max_len : remaining_len;

        //Fill
        rsp.attr_value.len = send_len;
        memcpy(rsp.attr_value.value, decs.value + param->read.offset, send_len);
    }
    //Normal read
    else
    {
        //Fill
        rsp.attr_value.len = decs.length;
        memcpy(rsp.attr_value.value, decs.value, rsp.attr_value.len);
    }
    
    *ret = ESP_GATT_OK;
    return &rsp;
}

/* Write blob inter buffer */
typedef struct {
    uint8_t*    prepare_buf;
    uint16_t    prepare_len;
    uint16_t    index;
} prepare_type_env_t;
static prepare_type_env_t prepare_write_env;

void example_prepare_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    static esp_gatt_rsp_t rsp;  

    //Prepare default responce
    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));  
    rsp.attr_value.handle = param->write.handle;  

    #if BLE_GATT_ECHO_LOG_DEGUG
        ESP_LOGI(TAG, "Prepare write, handle = %d, value len = %d", param->write.handle, param->write.len);
    #endif

    //Find index
    uint8_t indx = find_char_and_desr_index(param->read.handle);
    if( indx != ECHO_INDX_CHAR_IN_VAL   &&    //all writable
        indx != ECHO_INDX_CHAR_ECHO_VAL)
    {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_ERROR, &rsp);
        return;
    }

    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.offset > ECHO_GATTS_CHAR_VAL_LEN_MAX)
        status = ESP_GATT_INVALID_OFFSET;
    else if ((param->write.offset + param->write.len) > ECHO_GATTS_CHAR_VAL_LEN_MAX)
        status = ESP_GATT_INVALID_ATTR_LEN;

    if (status == ESP_GATT_OK && prepare_write_env->prepare_buf == NULL) 
    {
        prepare_write_env->prepare_buf = (uint8_t *)malloc(ECHO_GATTS_CHAR_VAL_LEN_MAX * sizeof(uint8_t));
        prepare_write_env->prepare_len = 0;
        if (prepare_write_env->prepare_buf == NULL) 
        {
            ESP_LOGE(TAG, "Write blob - prep no mem");
            status = ESP_GATT_NO_RESOURCES;
        }
    }

    //Send response if needed
    if (param->write.need_rsp)
    {
        rsp.attr_value.len = param->write.len;
        rsp.attr_value.handle = param->write.handle;
        rsp.attr_value.offset = param->write.offset;
        memcpy(rsp.attr_value.value, param->write.value, param->write.len);
        esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, &rsp);
        if (response_err != ESP_OK)
            ESP_LOGE(TAG, "Send response on write blob error");
    }
    if (status != ESP_GATT_OK)
        return;

    //Copy in internal buffer
    memcpy(prepare_write_env->prepare_buf + param->write.offset, param->write.value, param->write.len);
    prepare_write_env->prepare_len += param->write.len;
    prepare_write_env->index = indx;
}

void example_exec_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param)
{
    if (param->exec_write.exec_write_flag != ESP_GATT_PREP_WRITE_EXEC || prepare_write_env->prepare_buf == NULL)
    {
        ESP_LOGI(TAG,"ESP_GATT_PREP_WRITE_CANCEL");
    }
    else
    {
        //Write
        write_event(prepare_write_env->index, prepare_write_env->prepare_buf, prepare_write_env->prepare_len, param);
    }

    //Free buffer
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
    prepare_write_env->index = 0;
}


/****************************** GATT and GAP CALLBACK ******************************/

//Control Advertising 
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) 
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:             //When - advertising data set complete
        {
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);     //Do - start advertising
            }
        }break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:        //When - Scan response data set complete
        {
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);     //Do - Start advertising
            }
        }break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:                //Advertising started
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                    ESP_LOGE(TAG, "Advertising start failed");
                }else{
                    ESP_LOGI(TAG, "Advertising start successfully");
                }
            #endif
        }break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:                 //Advertising stopped
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                    ESP_LOGE(TAG, "Advertising stop failed");
                }
                else {
                    ESP_LOGI(TAG, "Advertising stop successfully");
                }
            #endif
        }break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:                //Client updates connection parameters
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                ESP_LOGI(TAG, "Update connection params status = %d, conn_int = %d, latency = %d, timeout = %d",
                    param->update_conn_params.status,
                    param->update_conn_params.conn_int,
                    param->update_conn_params.latency,
                    param->update_conn_params.timeout);
            #endif
        }break;
        default:
            break;
    }
}

#include "esp_timer.h"
uint32_t counter = 0;
int64_t time_old = 0;
int32_t connect_id = -1;

bool contin = true;

//GATT APP callback
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
   switch (event) {
        case ESP_GATTS_REG_EVT:
        {
            //Set name
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
            if (set_dev_name_ret){
                ESP_LOGE(TAG, "Set device name failed, error code = %x", set_dev_name_ret);
            }
            
            //Config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret){
                ESP_LOGE(TAG, "Config adv data failed, error code = %x", ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            
            //Config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret){
                ESP_LOGE(TAG, "Config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            
            //Create attribute table
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, ECHO_INDX_NB, ECHO_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(TAG, "Create attr table failed, error code = %x", create_attr_ret);
            }
        }break;

        case ESP_GATTS_READ_EVT:
        {   
            #if BLE_GATT_ECHO_LOG_DEGUG
                if(param->read.is_long == false)
                    ESP_LOGI(TAG, "ESP_GATTS_READ_EVT: handle=%d, conn_id=%d", param->read.handle, param->read.conn_id);
                else
                    ESP_LOGI(TAG, "ESP_GATTS_READ_EVT: LONG: handle=%d, conn_id=%d, offset=%d", param->read.handle, param->read.conn_id, param->read.offset);
            #endif

            //Find index
            uint8_t indx = find_char_and_desr_index(param->read.handle);

            //Action need only if setup manual control 
            if(gatt_db[indx].attr_control.auto_rsp == ESP_GATT_RSP_BY_APP)
            {
                //Prepare response
                esp_gatt_status_t ret;
                esp_gatt_rsp_t* rsp = read_and_read_blob(&ret, event, gatts_if, param);

                //Send response
                esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ret, rsp);
            }
        }break;

        case ESP_GATTS_WRITE_EVT:
        {
            //Write
            if (!param->write.is_prep)
            {   
                #if BLE_GATT_ECHO_LOG_DEGUG
                    if(param->write.is_prep == false)
                        ESP_LOGI(TAG, "GATT_WRITE_EVT: handle = %d, value len = %d", param->write.handle, param->write.len);
                    else
                        ESP_LOGI(TAG, "GATT_WRITE_EVT: LONG: handle = %d, value len = %d, offset=%d", param->write.handle, param->write.len,  param->write.offset);
                #endif

                //Find index
                uint8_t indx = find_char_and_desr_index(param->read.handle);
                
                //Write
                write_event(indx, param->write.value, param->write.len, param);  //send resp in function 
            }
            //Write blob
            else
                example_prepare_write_event_env(gatts_if, &prepare_write_env, param);   // send resp in function 
        }break;

        case ESP_GATTS_EXEC_WRITE_EVT:
        {
            ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT");

            //Execute write blob
            example_exec_write_event_env(gatts_if, &prepare_write_env, param);   // send resp in function 
        }break;

        case ESP_GATTS_MTU_EVT:
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            #endif
            
            mtu_size = param->mtu.mtu;
        }break;
        
        case ESP_GATTS_CONF_EVT:
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d, attr_handle %d", param->conf.status, param->conf.handle);
            #endif
        }break;
        
        case ESP_GATTS_START_EVT:
        {
            #if BLE_GATT_ECHO_LOG_DEGUG
                ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
            #endif
        }break;

        case ESP_GATTS_CONNECT_EVT:
        {
            ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            #if BLE_GATT_ECHO_LOG_DEGUG
                esp_log_buffer_hex(TAG, param->connect.remote_bda, 6);
            #endif
    
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));

            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x9;    // min_int = 0x9*1.25ms = 11.25ms - andrioid min interval
            conn_params.min_int = 0x9;    // min_int = 0x9*1.25ms = 11.25ms - andrioid min interval
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms

            //Start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
        }break;
        
        case ESP_GATTS_DISCONNECT_EVT:
        {
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);

            mtu_size = BLE_LOW_MTU_SIZE_DEFAULT;
            esp_ble_gap_start_advertising(&adv_params);

            //Reset cccid
            memset((uint8_t*)echo_ccc, 0, sizeof(echo_ccc));
            esp_ble_gatts_set_attr_value(echo_handle_table[ECHO_INDX_CHAR_ECHO_CFG], sizeof(echo_ccc), echo_ccc);

        }break;
        
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        {
            if (param->add_attr_tab.status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != ECHO_INDX_NB)
            {
                ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) \
                        doesn't equal to ECHO_INDX_NB(%d)", param->add_attr_tab.num_handle, ECHO_INDX_NB);
            }
            else 
            {
                #if BLE_GATT_ECHO_LOG_DEGUG
                    ESP_LOGI(TAG, "Create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                    memcpy(echo_handle_table, param->add_attr_tab.handles, sizeof(echo_handle_table));
                    esp_ble_gatts_start_service(echo_handle_table[ECHO_INDX_SVC]);
                #endif
            }
        }break;
        case ESP_GATTS_STOP_EVT:        //Not used
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}


//Main GATTS handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    //If event is register event, store the gatts_if for each profile
    if (event == ESP_GATTS_REG_EVT) 
    {
        if (param->reg.status == ESP_GATT_OK) 
            echo_profile_tab[ECHO_PROFILE_APP_IDX].gatts_if = gatts_if;
        else 
        {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    //Make callback to selected GATT APP
    do 
    {
        int idx;
        for (idx = 0; idx < ECHO_PROFILE_NUM; idx++) 
        {
            //ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == echo_profile_tab[idx].gatts_if) 
                if (echo_profile_tab[idx].gatts_cb) 
                    echo_profile_tab[idx].gatts_cb(event, gatts_if, param);
        }
    } while (0);
}

/****************************** START/STOP ******************************/

esp_err_t gatt_echo_start(void)
{
    esp_err_t ret = ble_start();
    if(ret != ESP_OK)
        return ret;
    
    return ble_gatt_register_app(ECHO_APP_ID, gatts_event_handler, gap_event_handler);
}

esp_err_t gatt_echo_stop(void)
{
    esp_err_t ret = ble_stop();
    if(ret != ESP_OK)
        return ret;

    return ESP_OK;
}
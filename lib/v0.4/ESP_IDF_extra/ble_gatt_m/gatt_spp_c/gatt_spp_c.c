#include "gatt_spp_c.h"
#include "m_ble_low.h"

static const char *TAG = "BLE_GATT_SPP_lib";

/****************************** GATTC UUID DEF ******************************/
#define SPP_GATTS_CHAR_VAL_LEN_MAX  240   //Characteristic max byte lenght

#define SPP_GATTS_SERVICE_UUID     0x00FF
#define SPP_GATTS_CHAR_OUT_UUID    0xFF01
#define SPP_GATTS_CHAR_IN_I_UUID   0xFF02
#define SPP_GATTS_CHAR_IN_N_UUID   0xFF03

//Init filters
static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPP_GATTS_SERVICE_UUID,},
};
static esp_bt_uuid_t remote_filter_char_out_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPP_GATTS_CHAR_OUT_UUID,},
};
static esp_bt_uuid_t remote_filter_char_in_i_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPP_GATTS_CHAR_IN_I_UUID,},
};
static esp_bt_uuid_t remote_filter_char_in_n_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = SPP_GATTS_CHAR_IN_N_UUID,},
};
static esp_bt_uuid_t remote_filter_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

/****************************** GAP SCAN DEF ******************************/
#define USE_NAME false
static const char remote_device_name[] = "HEART_SPP_SERVER";

#define USE_BDA (!USE_NAME) //Use BLE mac addresses
#define MAX_BDA 4
static uint8_t spp_server_bda[MAX_BDA][6] =
{
    [0] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    [1] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    [2] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    [3] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

//Scan parameters
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,    // Reduce the load? BLE_SCAN_FILTER_ALLOW_ONLY_WLST
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE    // Reduce the load? BLE 5.0 -> BLE_SCAN_DUPLICATE_ENABLE_RESET
};

/****************************** GATTC PROFILE DEF ******************************/
#define PROFILE_NUM          MAX_BDA
#define PROFILE_SPP_0_APP_ID 0
#define PROFILE_SPP_1_APP_ID 1
#define PROFILE_SPP_2_APP_ID 2
#define PROFILE_SPP_3_APP_ID 3
#define INVALID_HANDLE       0

//Declare proto
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t indx);
typedef void (*gattc_cb_custom) (esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t indx);

//Proto
struct gattc_profile_inst {
    gattc_cb_custom gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t write_char_handle;
    uint16_t indicate_char_handle;
    uint16_t notify_char_handle;
    esp_bd_addr_t remote_bda;
};

//One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTC_REG_EVT
static struct gattc_profile_inst SPP_C_profile_tab[PROFILE_NUM] = {
    [PROFILE_SPP_0_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       //Not get the gatt_if, so initial is ESP_GATT_IF_NONE
    },
    [PROFILE_SPP_1_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,
    },
    [PROFILE_SPP_2_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,
    },
    [PROFILE_SPP_3_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,
    },
};

/****************************** SPP FUCNTION ******************************/
#define SPP_C_NOTIF_FLAG        (1 << 0)
#define SPP_C_INDIC_FLAG        (1 << 1)

typedef struct 
{
    bool connect;
    bool get_server;
    uint8_t ready;
    uint16_t mtu;

    esp_gattc_char_elem_t *char_elem_result;
    esp_gattc_descr_elem_t *descr_elem_result;
}gatt_spp_c_connections_t;
gatt_spp_c_connections_t c_connections[MAX_BDA] = {0,};

static bool set_scan_param = false;

static QueueHandle_t spp_c_rx_queue[MAX_BDA] = {NULL,};
static QueueHandle_t spp_c_tx_queue = NULL;
static SemaphoreHandle_t send_sem[MAX_BDA] = {NULL,};

static TaskHandle_t spp_c_loop_task = NULL;
static bool spp_c_loop_task_enable = false;
static void spp_c_loop_tx();

void init_c_connections()
{
    for(uint8_t i=0; i<MAX_BDA; i++)
        c_connections[i].mtu = BLE_LOW_MTU_SIZE_DEFAULT;
}

static uint8_t indx_from_bda(uint8_t* bda)
{
    //Find index
    uint8_t i = 255;
    for(i=0; i<MAX_BDA; i++)
        if( memcmp(spp_server_bda[i], bda, 6) == 0)
            break;
    return i;
}

bool spp_set_bda_in_table(uint8_t* bda, uint8_t indx)
{
    if(indx>MAX_BDA)
        return false;
    
    //Copy in server table
    memcpy(spp_server_bda[indx], bda, 6);
    return true;
}

/****************************** INTERNAL FUCNTION ******************************/

//Helper function
static uint16_t get_attr_count(esp_gatt_if_t gattc_if, uint16_t conn_id, esp_gatt_db_attr_type_t db_type, uint16_t handle, uint8_t indx)
{
    uint16_t char_count = 0;
    esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if, conn_id, db_type,
                                                            SPP_C_profile_tab[indx].service_start_handle,
                                                            SPP_C_profile_tab[indx].service_end_handle,
                                                            handle, &char_count);
    if(status != ESP_GATT_OK)
        ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error [%d]", indx);
    return char_count;                                        
}

static esp_gatt_status_t char_by_uuid(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, esp_bt_uuid_t* filter, uint16_t* char_count, uint8_t indx)
{   
    esp_gatt_status_t status = esp_ble_gattc_get_char_by_uuid(gattc_if, param->search_cmpl.conn_id,
                                                              SPP_C_profile_tab[indx].service_start_handle,
                                                              SPP_C_profile_tab[indx].service_end_handle,
                                                              *filter,      //char filter
                                                              c_connections[indx].char_elem_result, char_count);
    return status;
}

static bool find_chars(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t indx)
{
    //Load characterisitcs numbers from cache
    uint16_t char_count = get_attr_count(gattc_if, param->search_cmpl.conn_id, ESP_GATT_DB_CHARACTERISTIC, INVALID_HANDLE, indx);
    if (char_count == 0)
    {
        ESP_LOGE(TAG, "No char found [%d]", indx);
        return false;
    }
    #if BLE_GATT_SPP_C_LOG_DEGUG
        ESP_LOGI(TAG, "In service [%d] found %d char:", indx, char_count);
    #endif

    //Find all char
    esp_bt_uuid_t* filters[3] = {&remote_filter_char_out_uuid, 
                                 &remote_filter_char_in_i_uuid, 
                                 &remote_filter_char_in_n_uuid};
    uint16_t* uuid_num[3] =    {&SPP_C_profile_tab[indx].write_char_handle, 
                                &SPP_C_profile_tab[indx].indicate_char_handle, 
                                &SPP_C_profile_tab[indx].notify_char_handle};
    char* char_names[3] = {"OUT", "IN_I", "IN_N"};
    (void)char_names;

    for(uint8_t i=0; i<3; i++)
    {
        //Create buffer
        c_connections[indx].char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * char_count);
        if (!c_connections[indx].char_elem_result)
            ESP_LOGE(TAG, "Gattc no mem [%d]", indx);
        else
        {   
            //Load characterisitcs from the cache
            uint16_t count = char_count;
            esp_gatt_status_t status = char_by_uuid(gattc_if, param, filters[i], &count, indx);
            if (status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error [%d]", indx);
                free(c_connections[indx].char_elem_result);
                return false;
            }

            //SPP service have only one char for every filters
            if (count > 0)
            {
                *(uuid_num[i]) = c_connections[indx].char_elem_result[0].char_handle;
                #if BLE_GATT_SPP_C_LOG_DEGUG
                    ESP_LOGI(TAG, "  [%d] %s char, handle=%d", indx, char_names[i], *(uuid_num[i]));
                #endif
            }
        }
        free(c_connections[indx].char_elem_result);
    }
    return true;
}

static bool reg_indic_notif(esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t indx)
{
    //Get count of descr
    uint16_t indic_notify_en = param->reg_for_notify.handle == SPP_C_profile_tab[indx].indicate_char_handle ? 2 : 1;
    uint16_t count = get_attr_count(gattc_if, SPP_C_profile_tab[indx].conn_id, 
                                            ESP_GATT_DB_DESCRIPTOR, param->reg_for_notify.handle, indx);
    if (count == 0)
    {
        ESP_LOGE(TAG, "Unexpected decsr [%d] not found!!", indx);
        return false;
    }

    c_connections[indx].descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
    if (!c_connections[indx].descr_elem_result)
    {
        ESP_LOGE(TAG, "Gattc no mem [%d]", indx);
        return false;
    }

    //Extract descr
    esp_gatt_status_t ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if, SPP_C_profile_tab[indx].conn_id,
                                                                        param->reg_for_notify.handle, remote_filter_descr_uuid,
                                                                        c_connections[indx].descr_elem_result, &count);
    if (ret_status != ESP_GATT_OK)
        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error [%d]", indx);

    //In SPP service every char have only one descr
    if (count > 0 && c_connections[indx].descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 
                  && c_connections[indx].descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
    {
        //Write to descr
        ret_status = esp_ble_gattc_write_char_descr(gattc_if, SPP_C_profile_tab[indx].conn_id,
                                                    c_connections[indx].descr_elem_result[0].handle, sizeof(indic_notify_en), (uint8_t *)&indic_notify_en,
                                                    ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        #if BLE_GATT_SPP_C_LOG_DEGUG
            ESP_LOGI(TAG, "Handle [%d] %d , enable notif/indic", indx, param->reg_for_notify.handle);
        #endif

        if (ret_status != ESP_GATT_OK)
        {   
            ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error [%d]", indx);
            //Reset flag
            c_connections[indx].ready &= param->reg_for_notify.handle == SPP_C_profile_tab[indx].indicate_char_handle ? ~SPP_C_INDIC_FLAG : ~SPP_C_NOTIF_FLAG;
        }
        //Set flag
        else
            c_connections[indx].ready |= param->reg_for_notify.handle == SPP_C_profile_tab[indx].indicate_char_handle ? SPP_C_INDIC_FLAG : SPP_C_NOTIF_FLAG;
    }

    free(c_connections[indx].descr_elem_result);
    return true;
}

/****************************** GATT and GAP CALLBACK ******************************/

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{   
    //Duration of scan
    static uint32_t duration = 10; //s
    //Device name
    static char adv_name[64];
    static uint8_t adv_name_len = 0;
    (void)remote_device_name;

    switch (event) 
    {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:   //When esp_ble_gap_set_scan_params is ok
        {   
            //Start scan
            esp_ble_gap_start_scanning(duration);
        }break;

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:       //When esp_ble_gap_start_scanning is ok
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
                    ESP_LOGE(TAG, "Scan start failed, error status = 0x%x", param->scan_start_cmpl.status);
            #endif
        }break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:               //When found device or timeout
        {   
            //Get type scan res
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;

            //Finded device
            if(scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT)
            {
                //Extract name
                uint8_t* ptr = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                if (ptr != NULL) 
                    snprintf(adv_name, (adv_name_len+1), "%s", ptr);

                #if BLE_GATT_SPP_C_SCAN_DEGUG
                    if(adv_name_len != 0)
                        ESP_LOGI(TAG, "Searched Device (name len=%d) Name = %s", adv_name_len, adv_name);
                    else
                        ESP_LOGI(TAG, "Searched Device without Name! bda = 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x", scan_result->scan_rst.bda[0],scan_result->scan_rst.bda[1],
                                                                                            scan_result->scan_rst.bda[2],scan_result->scan_rst.bda[3],
                                                                                            scan_result->scan_rst.bda[4],scan_result->scan_rst.bda[5]);
                #endif

                //Check name
                bool found = false;
                #if USE_NAME
                    uint8_t i = 0;
                    if (ptr != NULL && strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) 
                        found = true;
                #endif
                #if USE_BDA
                    uint8_t i = indx_from_bda( scan_result->scan_rst.bda);
                    if(i!=255) found = true;
                #endif

                if(found == false)
                    break;
                //Connect
                if (c_connections[i].connect != false) 
                    break;
                c_connections[i].connect = true;
                
                #if BLE_GATT_SPP_C_LOG_DEGUG
                    ESP_LOGI(TAG, "Searched device - %s", remote_device_name);
                    ESP_LOGI(TAG, "Try connect to the remote device [%d]", i);
                #endif

                esp_ble_gattc_open(SPP_C_profile_tab[i].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);                    
            }
            //Stop scan timeout
            else if(scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT)
            {   
                //ESP_LOGI(TAG, "Scan timeout, the device not found! Restart scan...");  FIXME: scan never stop
                
                //Retart scan
                esp_ble_gap_start_scanning(duration);
            }
        }break;

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:        //When esp_ble_gap_stop_scanning is ok
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
                    ESP_LOGE(TAG, "Scan stop failed, error status = 0x%x", param->scan_stop_cmpl.status);
                else
                    ESP_LOGI(TAG, "Stop scan successfully");
            #endif
        }break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:         // ?
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
                    ESP_LOGE(TAG, "Adv stop failed, error status = 0x%x", param->adv_stop_cmpl.status);
                else 
                    ESP_LOGI(TAG, "Stop adv successfully");
            #endif
        }break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:         //When updates connection parameters
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                uint8_t indx = indx_from_bda(param->update_conn_params.bda);
                ESP_LOGI(TAG, "Update connection [%d] params status = %d, min_int = %d, max_int = %d, conn_int = %d, latency = %d, timeout = %d",
                        indx,
                        param->update_conn_params.status,
                        param->update_conn_params.min_int,
                        param->update_conn_params.max_int,
                        param->update_conn_params.conn_int,
                        param->update_conn_params.latency,
                        param->update_conn_params.timeout);
            #endif
        }break;

        default:
            break;
    }
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t indx)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) 
    {
        case ESP_GATTC_REG_EVT:
        {   
            if(set_scan_param == false)
            {
                //Setup scan params
                esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
                if (scan_ret)
                    ESP_LOGE(TAG, "Set scan params error, error code = 0x%x", scan_ret);
                set_scan_param = true;
            }
        }break;

        case ESP_GATTC_CONNECT_EVT: 
            break;

        case ESP_GATTC_OPEN_EVT:
        {
            if (param->open.status != ESP_GATT_OK)
                ESP_LOGE(TAG, "Open-connection failed [%d], status %d",indx, p_data->open.status);
            else
                ESP_LOGI(TAG, "Open-connection success [%d]", indx);

            //Save parameters
            SPP_C_profile_tab[indx].conn_id = p_data->open.conn_id;
            memcpy(SPP_C_profile_tab[indx].remote_bda, p_data->open.remote_bda, sizeof(esp_bd_addr_t));

            #if BLE_GATT_SPP_C_LOG_DEGUG
                esp_log_buffer_hex(TAG, SPP_C_profile_tab[indx].remote_bda, sizeof(esp_bd_addr_t));
            #endif

            //Set mtu
            esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
            if (mtu_ret)
                ESP_LOGE(TAG, "config MTU error [%d], error code = 0x%x", indx, mtu_ret);

            //Allow tx
            if( send_sem[indx] != NULL)
                xSemaphoreGive(send_sem[indx]);
        }break;

        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        {
            if (param->dis_srvc_cmpl.status != ESP_GATT_OK)
                ESP_LOGE(TAG, "Discover service failed [%d], status %d", indx, param->dis_srvc_cmpl.status);
            else
            {
                #if BLE_GATT_SPP_C_LOG_DEGUG
                    ESP_LOGI(TAG, "Discover service complete [%d] conn_id %d. Load result...", indx, param->dis_srvc_cmpl.conn_id);
                #endif

                //Load result from local cache
                esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid);
            }
        }break;

        case ESP_GATTC_CFG_MTU_EVT:
        {
            if (param->cfg_mtu.status != ESP_GATT_OK)
                ESP_LOGE(TAG,"Config mtu failed [%d], error status = 0x%x", indx, param->cfg_mtu.status);
            #if BLE_GATT_SPP_C_LOG_DEGUG
                else
                    ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT [%d], Status OK, MTU %d, conn_id %d", indx, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
            #endif
            c_connections[indx].mtu = param->cfg_mtu.mtu;
        }break;

        case ESP_GATTC_SEARCH_RES_EVT: 
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "SEARCH RES [%d]: conn_id = 0x%x is primary service %d", indx, p_data->search_res.conn_id, p_data->search_res.is_primary);
                ESP_LOGI(TAG, "Handles [%d] from %d to %d. Current handle = %d", indx, p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
            #endif

            if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == SPP_GATTS_SERVICE_UUID) 
            {
                #if BLE_GATT_SPP_C_LOG_DEGUG
                    ESP_LOGI(TAG, "SPP service found [%d]!", indx);
                    ESP_LOGI(TAG, "UUID16 [%d]: 0x%x", indx, p_data->search_res.srvc_id.uuid.uuid.uuid16);
                #endif
                
                //Save service start and stop handles
                c_connections[indx].get_server = true;
                SPP_C_profile_tab[indx].service_start_handle = p_data->search_res.start_handle;
                SPP_C_profile_tab[indx].service_end_handle = p_data->search_res.end_handle;
            }
            else
                ESP_LOGE(TAG, "SPP service [%d] not found!",indx);
        }break;

        case ESP_GATTC_SEARCH_CMPL_EVT:
        {   
            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT [%d]", indx);
            #endif
            //Check status
            if (p_data->search_cmpl.status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Search service failed [%d], error status = 0x%x", indx, p_data->search_cmpl.status);
                break;
            }

            if (!c_connections[indx].get_server)
                break;
            
            //Find and mem all char
            if( find_chars(gattc_if, param, indx) == false)
                break; 

            //If all ok, registers
            esp_ble_gattc_register_for_notify(gattc_if, SPP_C_profile_tab[indx].remote_bda, SPP_C_profile_tab[indx].indicate_char_handle);
            esp_ble_gattc_register_for_notify(gattc_if, SPP_C_profile_tab[indx].remote_bda, SPP_C_profile_tab[indx].notify_char_handle);
        }break;

        case ESP_GATTC_REG_FOR_NOTIFY_EVT: 
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT [%d], HANDLE = %d", indx, p_data->reg_for_notify.handle);
            #endif

            if (p_data->reg_for_notify.status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "REG FOR NOTIFY failed [%d]: error status = %d", indx, p_data->reg_for_notify.status);
                break;
            }

            //Register and enable indication and notification 
            reg_indic_notif(gattc_if, param, indx);
        }break;

        case ESP_GATTC_NOTIFY_EVT:
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                if (p_data->notify.is_notify)
                    ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT [%d], receive notify value:", indx);
                else
                    ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT [%d], receive indicate value:", indx);
                esp_log_buffer_hex(TAG, p_data->notify.value, p_data->notify.value_len);
            #endif

            //Add to queue
            if(spp_c_rx_queue[indx] != NULL)
            {
                m_spp_c_data_block_t rx_block;

                rx_block.indx = indx;
                rx_block.length = p_data->notify.value_len;
                memcpy(rx_block.data, p_data->notify.value, p_data->notify.value_len);
                xQueueSend(spp_c_rx_queue[indx] , (void*)&rx_block, 0);   //No delay
            }

        }break;

        case ESP_GATTC_WRITE_DESCR_EVT:
        {
            if (p_data->write.status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Write descr failed [%d] (handle=%d), error status = 0x%x", indx, p_data->write.handle, p_data->write.status);
                break;
            }
            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "Write descr success [%d] (handle=%d)", indx, p_data->write.handle);
            #endif
        }break;

        case ESP_GATTC_SRVC_CHG_EVT: 
        {
            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT [%d], bd_addr:", indx);
                esp_log_buffer_hex(TAG, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
            #endif
        }break;

        case ESP_GATTC_WRITE_CHAR_EVT:
        {
            if (p_data->write.status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "Write char failed [%d] (handle=%d), error status = 0x%x", indx, p_data->write.handle, p_data->write.status);
                break;
            }

            //Allow send next msg
            if( send_sem[indx] != NULL)
                xSemaphoreGive(send_sem[indx]);

            #if BLE_GATT_SPP_C_LOG_DEGUG
                ESP_LOGI(TAG, "Write char success [%d] (handle=%d)", indx, p_data->write.handle);
            #endif
        }break;
        
        case ESP_GATTC_DISCONNECT_EVT: break;
        case ESP_GATTC_CLOSE_EVT:
        {
            //Reset param
            c_connections[indx].connect    = false;
            c_connections[indx].get_server = false;
            c_connections[indx].ready      = 0;
            c_connections[indx].mtu = BLE_LOW_MTU_SIZE_DEFAULT;

            ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT [%d], reason = %d", indx, p_data->disconnect.reason);
        }break;

        default:
            break;
    }
}

//. . . . . . . . . . . . . . . . . . . ,,_ . . . . . . . . ,-'`-,
//. . . . . . . . . . . . . . . . . . . ..,, '``-.__.,.,./ .,., .|
//. . . . . . . . . . . . . . . . . . . . .|,.-;`;` . . ,.,., .,.,.,`-,
//..,,,.,.,._ . . . . . . . . . . . . . . / :O;. . .;O; .['. . . . `'-.,
//['. . . . . . '`'`'`'`'*-----,.,.,.,._| . . . -;- . . . . '`-,._ . . . `'-.,
//'``'*----,,,,,.,.,.,.,_. . . . . . . ..|,, . ,-,-,. . . . . ,.,. `'-, . . . .|,
//. . . . . . . . . . . . . . ``'`'*----.,., .`'`-----'`'`'`` . . . . .`-. . . . |`
//. . . . . . . . . . . . . . . . . . . . . .`'-. . . . . . . . . . . . . . . . . . .,|
//. . . . . . . . . . . . . . . . . . . . . . . |;. . . . . . . . . . . . . . . . . ,|
//. . . . . . . . . . . . . . . . . . . . . . . |;. . . . . . . . . . . . . . . . .,|
static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    //If event is register event, store the gattc_if for each profile
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) 
            SPP_C_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        else 
        {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    //Make callback to selected GATTC APP
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) 
            //ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function
            if (gattc_if == ESP_GATT_IF_NONE || gattc_if == SPP_C_profile_tab[idx].gattc_if) 
                if (SPP_C_profile_tab[idx].gattc_cb)
                    SPP_C_profile_tab[idx].gattc_cb(event, gattc_if, param, idx);
    } while (0);
}

/****************************** CONTROL ******************************/
esp_err_t gatt_spp_c_start(BaseType_t core)
{
//Reset params
    memset(c_connections, 0, sizeof(c_connections));
    set_scan_param = false; 
    init_c_connections();
    
 //Start BLE
    esp_err_t ret = ble_start();
    if(ret != ESP_OK)
        return ret;
    
    //Register app
    ret = ble_gattc_register_app(PROFILE_SPP_0_APP_ID, gattc_event_handler, gap_event_handler);
    if(ret != ESP_OK)
        return ret;

    ret = ble_gattc_register_app(PROFILE_SPP_1_APP_ID, gattc_event_handler, gap_event_handler);
    if(ret != ESP_OK)
        return ret;

    ret = ble_gattc_register_app(PROFILE_SPP_2_APP_ID, gattc_event_handler, gap_event_handler);
    if(ret != ESP_OK)
        return ret;

    ret = ble_gattc_register_app(PROFILE_SPP_3_APP_ID, gattc_event_handler, gap_event_handler);
    if(ret != ESP_OK)
        return ret;

    //Check running task
    if(spp_c_loop_task != NULL)
    {
        ESP_LOGW(TAG, "SPPC Task is already started!");
        return ESP_FAIL;
    }

    //Create task
    spp_c_loop_task_enable  = true;
    xTaskCreatePinnedToCore(&spp_c_loop_tx, "spp_c_loop_tx", 8192, NULL, 5, &spp_c_loop_task, core);

    return ESP_OK;
}

esp_err_t gatt_spp_c_stop(void)
{
    //Stop BLE, clear mem = delete app
    ble_stop();

    //Check running task
    if(spp_c_loop_task == NULL)
    {
        ESP_LOGW(TAG, "SPPC Task is already stopped!");
        return ESP_FAIL;
    }

    //Create request to stop task
    spp_c_loop_task_enable = false;

    //Timeout 100*50 = 5000 ms
    for(uint16_t i=0; i<100; i++)
    {
        if(spp_c_loop_task == NULL)
            break;
        vTaskDelay(100/portTICK_PERIOD_MS);
    }

    //Not stopped
    if(spp_c_loop_task != NULL)
    {
        ESP_LOGE(TAG, "SPPC Task hasn't been stopped!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void spp_c_loop_tx(void *pvParameters)
{
    //Create queues
    for(uint8_t i=0; i<MAX_BDA; i++)
        spp_c_rx_queue[i] = xQueueCreate(5, sizeof(m_spp_c_data_block_t)); 
    spp_c_tx_queue = xQueueCreate(5*MAX_BDA, sizeof(m_spp_c_data_block_t));

    //Create semaphores
    for(uint8_t i=0; i<MAX_BDA; i++)
    {
        send_sem[i] = xSemaphoreCreateBinary();
        xSemaphoreGive(send_sem[i]);
    }

    //Prepare block
    m_spp_c_data_block_t tx_block = {0,};

    //Loop
    while(spp_c_loop_task_enable)
    {
        //Wait for block in queue
        if(xQueueReceive(spp_c_tx_queue, (void*)&tx_block, pdMS_TO_TICKS(50) ) != pdPASS)
            continue;

        //Truncate len
        if(tx_block.length > SPP_GATTS_CHAR_VAL_LEN_MAX)
            tx_block.length = SPP_GATTS_CHAR_VAL_LEN_MAX;

        //Select type
        esp_gatt_write_type_t msg_type = tx_block.use_noreply ? ESP_GATT_WRITE_TYPE_NO_RSP : ESP_GATT_WRITE_TYPE_RSP;

        //Extract device index
        uint8_t indx = tx_block.indx;

        //Try send
        uint16_t total_sent = 0;
        uint8_t abort = 20; //~1 sec
        while ( (total_sent < tx_block.length) && spp_c_loop_task_enable && c_connections[indx].ready) 
        {   
            if(xSemaphoreTake(send_sem[indx], pdMS_TO_TICKS(50)) != pdPASS)
            {
                if(--abort == 0)
                    break;
                else
                    continue;;
            }

            //Cacl len
            uint16_t max_len = c_connections[indx].mtu - 3;    //Header size 3 byte
            uint16_t remaining_len = tx_block.length - total_sent;
            uint16_t send_len = (remaining_len > max_len) ? max_len : remaining_len;

            //Send
            esp_err_t ret = esp_ble_gattc_write_char( SPP_C_profile_tab[indx].gattc_if, SPP_C_profile_tab[indx].conn_id,
                                                      SPP_C_profile_tab[indx].write_char_handle, send_len,
                                                      tx_block.data + total_sent,
                                                      msg_type, ESP_GATT_AUTH_REQ_NONE);
            if (ret!= ESP_OK)
                break;

            total_sent += send_len;
        }
    }

    //Delete queue
    for(uint8_t i=0; i<MAX_BDA; i++)
    {
        vQueueDelete(spp_c_rx_queue[i]); 
        spp_c_rx_queue[i] = NULL;
    }
    vQueueDelete(spp_c_tx_queue);
    spp_c_tx_queue = NULL;

    //Delete sem
    for(uint8_t i=0; i<MAX_BDA; i++)
    {
        vSemaphoreDelete(send_sem[i]);
        send_sem[i] = NULL;
    }

    //Task stop
    ESP_LOGI(TAG, "SPP Task is stopped!");

    //Stopped
    spp_c_loop_task = NULL;
    vTaskDelete(spp_c_loop_task);
}

esp_err_t  gatt_spp_c_copy_rx(m_spp_c_data_block_t* rx_block)
{     
    uint8_t indx = rx_block->indx;
    if(spp_c_rx_queue[indx] == NULL)
    {
        ESP_LOGE(TAG, "SPP_C Task is not started!");
        return ESP_FAIL;
    }

    if(xQueueReceive(spp_c_rx_queue[indx], (void*)rx_block, 0) != pdTRUE)
    {
        rx_block->length = 0;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t  gatt_spp_c_request_send(m_spp_c_data_block_t* tx_block)
{
    if(spp_c_tx_queue == NULL)
    {
        ESP_LOGE(TAG, "SPP Task is not started!");
        return ESP_FAIL;
    }

    //Add to queue
    if(xQueueSend(spp_c_tx_queue, tx_block, pdMS_TO_TICKS(1000) ) != pdPASS)
    {
        ESP_LOGE(TAG, "Queue TX is full!");
        return ESP_FAIL;
    }

    return ESP_OK;
}
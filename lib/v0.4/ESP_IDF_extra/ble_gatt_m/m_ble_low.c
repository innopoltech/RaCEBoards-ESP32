#include "m_ble_low.h"
static const char *TAG = "BLE_low";

static bool vhci_controller_init    = false;
static bool vhci_controller_enable  = false;
static bool bluedroid_init          = false;
static bool bluedroid_enable        = false;

static bool ble_started = false;
static uint16_t ble_mtu_len = BLE_LOW_MTU_SIZE_DEFAULT;

esp_err_t ble_start()
{
    if(ble_started == true)
    {
        ESP_LOGW(TAG, "BLE is already started => restart...");
    }

    //Try stop
    ble_stop();

    ESP_LOGI(TAG, "BLE try to start");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {   
        //Erase
        if(nvs_flash_erase() != ESP_OK)
        {
            ESP_LOGE(TAG, "Not able connect - NVS error");
            return ESP_FAIL;
        }

        //Try init
        if(nvs_flash_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Not able connect - NVS error");
            return ESP_FAIL;
        }
    }

    //Init VHCI controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "%s init controller failed: %s", __func__, esp_err_to_name(ret));
        goto error;
    }
    vhci_controller_init = true;
    
    //Enable VHCI controller
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        goto error;
    }
    vhci_controller_enable = true;

    //Bluedriod init
    ret = esp_bluedroid_init();
    if (ret) 
    {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        goto error;
    }
    bluedroid_init = true;

    //Bluedriod enable
    ret = esp_bluedroid_enable();
    if (ret) 
    {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        goto error;
    }
    bluedroid_enable = true;
 
    //Set MTU
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(BLE_LOW_MTU_SIZE);
    if (local_mtu_ret)
    {
        ESP_LOGE(TAG, "Set local  MTU failed, error code = %x", local_mtu_ret);
        ble_mtu_len = BLE_LOW_MTU_SIZE_DEFAULT;
    }
    else{
        ble_mtu_len = BLE_LOW_MTU_SIZE;
    }

    ESP_LOGI(TAG, "BLE started");
    ble_started = true;
    return ESP_OK;

error:
    ble_stop();
    return ESP_FAIL;
}


esp_err_t ble_stop()
{   
    ble_started = false;
    esp_err_t ret;

    ESP_LOGI(TAG, "BLE try to stop");

    //Disable bluedriod
    if(bluedroid_enable)
    {
        ret = esp_bluedroid_disable();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) 
        {
            ESP_LOGE(TAG, "%s disable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        }
        bluedroid_enable = false;
    }

    //Deinit bluedriod
    if(bluedroid_init)
    {
        ret = esp_bluedroid_deinit();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) 
        {
            ESP_LOGE(TAG, "%s deinit bluetooth failed: %s", __func__, esp_err_to_name(ret));
        }
        bluedroid_init = false;
    }

    //Disable VHCI controller
    if(vhci_controller_enable)
    {
        ret = esp_bt_controller_disable();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)  
        {
            ESP_LOGE(TAG, "%s disable controller failed: %s", __func__, esp_err_to_name(ret));
        }
        vhci_controller_enable = false;
    }

    //Deinit VHCI controller
    if(vhci_controller_init)
    {
        ret = esp_bt_controller_deinit();
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)  
        {
            ESP_LOGE(TAG, "%s deinit controller failed: %s", __func__, esp_err_to_name(ret));
        }
        vhci_controller_init = false;
    }

    //Clear mem
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE stopped");
    return ESP_OK;
}

esp_err_t ble_gatt_register_app(uint32_t app_id, gatts_event_handler_f gatts_event_handler, gap_event_handler_f gap_event_handler)
{
    if(ble_started == false)
    {
        ESP_LOGW(TAG, "BLE not started! Unable to register APP");
        return ESP_FAIL;
    }

    //Register GATT callback
    esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GATTS register error, error code = %x", ret);
        return ESP_FAIL;
    }

    //Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GAP register error, error code = %x", ret);
        return ESP_FAIL;
    }

    //Register APP
    ret = esp_ble_gatts_app_register(app_id);
    if (ret){
        ESP_LOGE(TAG, "GATTS app register error, error code = %x", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE registered APP-%lu", app_id);
    return ESP_OK;
}

esp_err_t ble_gattc_register_app(uint32_t app_id, gattc_event_handler_f gattc_event_handler, gap_event_handler_f gap_event_handler)
{
    if(ble_started == false)
    {
        ESP_LOGW(TAG, "BLE not started! Unable to register APP");
        return ESP_FAIL;
    }

    //Register GATTC callback
    esp_err_t ret = esp_ble_gattc_register_callback(gattc_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GATTS register error, error code = %x", ret);
        return ESP_FAIL;
    }

    //Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GAP register error, error code = %x", ret);
        return ESP_FAIL;
    }

    //Register client APP
    ret = esp_ble_gattc_app_register(app_id);
    if (ret){
        ESP_LOGE(TAG, "GATTS app register error, error code = %x", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "BLE clinet registered APP-%lu", app_id);
    return ESP_OK;
}
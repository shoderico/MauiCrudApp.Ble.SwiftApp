#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#define MAIN_TAG "GATTS_DEMO"



#define SERVICE_UUID      0x9E,0xCA,0xDC,0x24,0x0E,0xE5,   0xA9,0xE0,0xF3,0x93,0xA3,0xB5,   0x01,   0x00,0x40,0x6E
#define NOTIFY_CHAR_UUID  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,   0xA9,0xE0,0xF3,0x93,0xA3,0xB5,   0x02,   0x00,0x40,0x6E
#define WRITE_CHAR_UUID   0x9E,0xCA,0xDC,0x24,0x0E,0xE5,   0xA9,0xE0,0xF3,0x93,0xA3,0xB5,   0x03,   0x00,0x40,0x6E
#define DEVICE_NAME       'S', 'w', 'i', 'f', 't', 'D', 'e', 'v', 'i', 'c', 'e'
#define DEVICE_NAME_LEN   11

// Advertise parameter
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20, // advertising interval max (32ms)
    .adv_int_max = 0x40, // advertising interval max (64ms)
    .adv_type = ADV_TYPE_IND, // connectable advertise
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC, // public address
    .channel_map = ADV_CHNL_ALL, // use all channels
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY, // allow scan & connectable
};

// advertise data : must be <= 31bytes
static uint8_t adv_data[] = {

    // Flags (general discovery mode)
    0x02, // Length: Type (1byte) + Flags (1byte) = 2
    ESP_BLE_AD_TYPE_FLAG, // Type:   0x01: discoverability, connectability ESP_BLE_AD_TYPE_FLAG
    0x06, // Flags:  0x06 = 0b 0000 0110
          //                         ^^^ 
          // bit0: LE Limited Discoverable Mode : '0' -> Not Supported
          // bit1: LE General Discoverable Mode : '1' -> Supported
          // bit2: BR/EDR Not Supported         : '1' -> Non-BR/EDR => BLE only

    // Device name
    0x0C, // Length: Type (1byte) + length of device name (11) = 12
    ESP_BLE_AD_TYPE_NAME_CMPL, // Type:   0x09: Complete Local Name  ESP_BLE_AD_TYPE_NAME_CMPL
    DEVICE_NAME,

    // Service UUID (128bit)
    //0x11, // Length: Type (1byte) + length of UUID (16bytes) = 17
    //ESP_BLE_AD_TYPE_128SRV_CMPL, // Type  : 0x07: Complete 128-bit Service UUID
    //SERVICE_UUID

    // TX Power Level
    0x02,
    ESP_BLE_AD_TYPE_TX_PWR,
    0xEB     // Length 2, Data Type ESP_BLE_AD_TYPE_TX_PWR, Data 2 (-21)
};

// Service UUID
static uint8_t service_uuid[16] = { SERVICE_UUID };
// Characteristic UUID : Notify
static uint8_t notify_char_uuid[16] = { NOTIFY_CHAR_UUID };
// Characteristic UUID : Write
static uint8_t write_char_uuid[16] = { WRITE_CHAR_UUID };


static uint8_t raw_scan_rsp_data[] = {
    // Service UUID (128bit)
    0x11, // Length: Type (1byte) + length of UUID (16bytes) = 17
    ESP_BLE_AD_TYPE_128SRV_CMPL, // Type  : 0x07: Complete 128-bit Service UUID
    SERVICE_UUID

    // Complete Local Name
    //0x09, // Length: Type (1byte) + length of device name
    //ESP_BLE_AD_TYPE_NAME_CMPL,
    //DEVICE_NAME,
};

//-----------------------------------------------------------------------------
// GATT handles and ids
struct my_gatt_handles_and_ids_t
{
    esp_gatt_if_t gatt_if;
    uint16_t gatt_service_handle;
    uint16_t gatt_notify_char_handle;
    uint16_t gatt_cccd_handle;
    uint16_t gatt_write_char_handle;
    uint16_t gatt_conn_id;
    esp_gatt_srvc_id_t serviceid;
    esp_bt_uuid_t notify_charuuid;
    esp_bt_uuid_t write_charuuid;
    esp_bt_uuid_t descruuid;
    bool is_notify_enabled;
};
static struct my_gatt_handles_and_ids_t gatt_info = {
    .gatt_if = ESP_GATT_IF_NONE,
    .gatt_service_handle = 0,
    .gatt_notify_char_handle = 0,
    .gatt_cccd_handle = 0,
    .gatt_write_char_handle = 0,
    .gatt_conn_id = 0,
    .is_notify_enabled = false,
};




//---------------------------------------------------------------------
// LED
#define LED_GPIO 21
static uint16_t s_led_state = 0;

// PWM Setting
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // 13 bit resolution (0 - 8191)
#define LEDC_FREQUENCY 1000 // PWM frequency 1kHz
#define FADE_TIME 30 // 30ms interval
#define MAX_DUTY 34 // max duty ( cycle per seconds )

static void blink_led(void)
{
    // Scaling : s_led_state (0 - 34) map to (0 - 8191)
    uint32_t scaled_duty = (s_led_state * 8191) / MAX_DUTY;

    // Active Low : invert duty
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 8191 - scaled_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL));
}

static void configure_led(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 8191, // initial off (active low)
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}



//-----------------------------------------------------------------------------
// Notify
static TimerHandle_t notify_timer;
static void notify_timer_callback(TimerHandle_t xTimer) {
    if (gatt_info.is_notify_enabled && gatt_info.gatt_notify_char_handle != 0) {

        // 10 bytes dummy data
        static uint32_t counter = 0;
        uint8_t data[10] = {0};
        data[0] = (uint8_t)(counter & 0xFF);
        data[1] = (uint8_t)((counter >> 8) & 0xFF);
        counter++;
        
        //ESP_LOGI(MAIN_TAG, "Sending notify: %"PRIx32"", counter);
        esp_ble_gatts_send_indicate(gatt_info.gatt_if, gatt_info.gatt_conn_id, gatt_info.gatt_notify_char_handle, 10, data, false);
    }
}

//-----------------------------------------------------------------------------
// Write
static QueueHandle_t write_data_queue;
static TimerHandle_t write_data_read_timer;
static void write_data_read_timer_callback(TimerHandle_t xTimer)
{
    uint8_t data[10];
    // peek recent data from queue (non-blocking)
    if (xQueueReceive(write_data_queue, data, 0) == pdTRUE) {

        uint16_t counter = (uint16_t)(data[0] | (data[1] << 8));
        uint32_t duty = counter % (2 * MAX_DUTY);
        if (duty >= MAX_DUTY) {
            duty = 2 * MAX_DUTY - duty - 1;
        }
        s_led_state = duty;

        blink_led();
    }
}



//-----------------------------------------------------------------------------
// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: // seq[b-2]
            ESP_LOGI(MAIN_TAG, "GAP : Advertising data set complete");
            
            // start advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;
        
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(MAIN_TAG, "GAP : Advertising started");
            } else {
                ESP_LOGE(MAIN_TAG, "GAP : Advertising start failed");
            }
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            ESP_LOGI(MAIN_TAG, "GAP : ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT");
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(MAIN_TAG, "GAP : ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
            break;
            
        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            ESP_LOGI(MAIN_TAG, "GAP : ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT");
            break;
            
        case ESP_GAP_BLE_CHANNEL_SELECT_ALGORITHM_EVT:
            ESP_LOGI(MAIN_TAG, "GAP : ESP_GAP_BLE_CHANNEL_SELECT_ALGORITHM_EVT");
            break;;

        default:
            ESP_LOGI(MAIN_TAG, "GAP : Unhandled GAP event: %d", event);
            break;
    }
}


//-----------------------------------------------------------------------------
// gatt event handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_err_t ret;
    switch (event) {
        case ESP_GATTS_REG_EVT: // seq[a-2]
            ESP_LOGI(MAIN_TAG, "GATT: GATT app registered, status=%d", param->reg.status);
            if (param->reg.status == ESP_GATT_OK) {


                // set advertise data
                ESP_LOGI(MAIN_TAG, "GATT: Configuring adv data");
                ret = esp_ble_gap_config_adv_data_raw(adv_data, sizeof(adv_data)); // triggers ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT. seq[b-1]
                if (ret) {
                    ESP_LOGE(MAIN_TAG, "%s config raw adv data failed, error code = %s ", __func__, esp_err_to_name(ret));
                }

                // set scan response data
                esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
                if (raw_scan_ret) {
                    ESP_LOGE(MAIN_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
                }

                // gatt interface
                gatt_info.gatt_if = gatts_if;

                // prepare service id
                gatt_info.serviceid.id.inst_id = 0;
                gatt_info.serviceid.is_primary = true;
                gatt_info.serviceid.id.uuid.len = ESP_UUID_LEN_128;
                memcpy(gatt_info.serviceid.id.uuid.uuid.uuid128, service_uuid, sizeof(service_uuid));
                
                // Create a GATT Server service. // triggers ESP_GATTS_CREATE_EVT.
                esp_ble_gatts_create_service(gatts_if, &gatt_info.serviceid, 12); // 12: The number of handles requested for this service.
            }
            break;

        case ESP_GATTS_CREATE_EVT: // seq[a-3]
            ESP_LOGI(MAIN_TAG, "GATT: Service created, handle=%d", param->create.service_handle);

            // gatt service handle
            gatt_info.gatt_service_handle = param->create.service_handle;

            // prepare notify characteristic uuid
            gatt_info.notify_charuuid.len = ESP_UUID_LEN_128;
            memcpy(gatt_info.notify_charuuid.uuid.uuid128, notify_char_uuid, sizeof(notify_char_uuid));
            
            // Add a characteristic into a service. // triggers ESP_GATTS_ADD_CHAR_EVT.
            esp_ble_gatts_add_char(gatt_info.gatt_service_handle
                , &gatt_info.notify_charuuid
                , ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE
                , ESP_GATT_CHAR_PROP_BIT_NOTIFY // | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE
                , NULL
                , NULL
            );
            break;

        case ESP_GATTS_ADD_CHAR_EVT: // seq[a-4], seq[a-6]
            if (param->add_char.status == ESP_GATT_OK) {
                if (!gatt_info.gatt_notify_char_handle) {
                    ESP_LOGI(MAIN_TAG, "GATT: Characteristic added, handle=%d", param->add_char.attr_handle);

                    // gatt notify characteristic handle
                    gatt_info.gatt_notify_char_handle = param->add_char.attr_handle;

                    uint16_t length = 0;
                    const uint8_t *prf_char;
                    ret = esp_ble_gatts_get_attr_value(gatt_info.gatt_notify_char_handle, &length, &prf_char);
                    if (ret == ESP_FAIL){
                        ESP_LOGE(MAIN_TAG, "GATT: ILLEGAL HANDLE");
                    }

                    ESP_LOGI(MAIN_TAG, "GATT: the gatts data length = %x", length);
                    for (int i = 0; i < length; i++) {
                        ESP_LOGI(MAIN_TAG, "GATT: prf_char[%x] =%x",i,prf_char[i]);
                    }

                    // prepare description uuid
                    gatt_info.descruuid.len = ESP_UUID_LEN_16;
                    gatt_info.descruuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

                    // Add a characteristic descriptor. // triggers ESP_GATTS_ADD_CHAR_DESCR_EVT.
                    ret = esp_ble_gatts_add_char_descr(gatt_info.gatt_service_handle
                        , &gatt_info.descruuid
                        , ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE
                        , NULL
                        , NULL);
                    if (ret) {
                        ESP_LOGE(MAIN_TAG, "GATT: %s Add a characteristic descriptor failed, error code = %s", __func__, esp_err_to_name(ret));
                    }
                } else {
                    ESP_LOGI(MAIN_TAG, "GATT: Write characteristic added, handle=%d", param->add_char.attr_handle);
                    gatt_info.gatt_write_char_handle = param->add_char.attr_handle;

                    // Start a service. // triggers ESP_GATTS_START_EVT.
                    esp_ble_gatts_start_service(gatt_info.gatt_service_handle);
                }

            } else {
                ESP_LOGE(MAIN_TAG, "GATT: Add characteristic failed, status=%d", param->add_char.status);
            }
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT: // seq[a-5]
            if (param->add_char_descr.status == ESP_GATT_OK) {
                ESP_LOGI(MAIN_TAG, "GATT: CCCD added, handle=%d", param->add_char_descr.attr_handle);

                // cccd handle
                gatt_info.gatt_cccd_handle = param->add_char_descr.attr_handle;

                // Write characteristic // triggers ESP_GATTS_ADD_CHAR_EVT.
                gatt_info.write_charuuid.len = ESP_UUID_LEN_128;
                memcpy(gatt_info.write_charuuid.uuid.uuid128, write_char_uuid, sizeof(write_char_uuid));
                ret = esp_ble_gatts_add_char(gatt_info.gatt_service_handle
                    , &gatt_info.write_charuuid
                    , ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE
                    , ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR
                    , NULL
                    , NULL
                );
                if (ret) {
                    ESP_LOGE(MAIN_TAG, "GATT: Add Write characteristic failed, error code = %s", esp_err_to_name(ret));
                }
            } else {
                ESP_LOGE(MAIN_TAG, "GATT: Add CCCD failed, status=%d (0x%x)", param->add_char_descr.status, param->add_char_descr.status);
            }
            break;

        case ESP_GATTS_START_EVT: // seq[a-7]
            ESP_LOGI(MAIN_TAG, "GATT: Service started, handle=%d", param->start.service_handle);
            break;

        
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(MAIN_TAG, "GATT: Client connected, conn_id=%d", param->connect.conn_id);
            gatt_info.gatt_conn_id = param->connect.conn_id;

            esp_bd_addr_t bd_addr;
            memcpy(bd_addr, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            ret = esp_ble_gap_set_prefer_conn_params(
                bd_addr
                , 0x06 // min interval: 7.5ms
                , 0x0C // max interva: 15ms
                , 0    // latency : 0 sec
                , 400  // timeout : 4 sec
            );
            if (ret != ESP_OK) {
                ESP_LOGE(MAIN_TAG, "GATT: Set preferred connection params failed: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(MAIN_TAG, "GATT: Set preferred connection params: min_int=7.5ms, max_int=15ms");
            }
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(MAIN_TAG, "GATT: Client disconnected");
            gatt_info.is_notify_enabled = false;
            gatt_info.gatt_conn_id = 0;

            // re-start advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;



        case ESP_GATTS_WRITE_EVT:
            // Write to CCCD
            if (param->write.handle == gatt_info.gatt_cccd_handle) {
                if (param->write.len == 2 && param->write.value[0] == 0x01 && param->write.value[1] == 0x00) {
                    ESP_LOGI(MAIN_TAG, "GATT: Notify enabled");
                    gatt_info.is_notify_enabled = true;

                    // Start notify timer
                    xTimerStart(notify_timer, 0);

                } else if (param->write.len == 2 && param->write.value[0] == 0x00 && param->write.value[1] == 0x00) {
                    ESP_LOGI(MAIN_TAG, "GATT: Notify disabled");
                    gatt_info.is_notify_enabled = false;

                    // Stop notify timer
                    xTimerStop(notify_timer, 0);
                }

                if (param->write.need_rsp) {
                    ESP_LOGI(MAIN_TAG, "GATT: Notify Send Response");
                    esp_ble_gatts_send_response(gatt_info.gatt_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            } else if (param->write.handle == gatt_info.gatt_write_char_handle) {
                
                // Write Characteristic
                //ESP_LOGI(MAIN_TAG, "GATT: Write to Write characteristic, len=%d", param->write.len);
                
                if (param->write.len <= 10) {

                    uint8_t queue_data[10] = {0}; // 10 bytes
                    memcpy(queue_data, param->write.value, param->write.len);
                    
                    // Overwrite queue with recent data
                    if (xQueueOverwrite(write_data_queue, queue_data) != pdTRUE) {
                        ESP_LOGE(MAIN_TAG, "Failed to overwrite queue");
                    }
                    //ESP_LOGI(MAIN_TAG, "Write received, len=%d, data[0]=0x%02x", param->write.len, queue_data[0]);

                } else {
                    ESP_LOGW(MAIN_TAG, "GATT: Write data too long, len=%d", param->write.len);
                }
                if (param->write.need_rsp) {
                    ESP_LOGI(MAIN_TAG, "GATT: Write Send Response");
                    esp_ble_gatts_send_response(gatt_info.gatt_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(MAIN_TAG, "GATT: ESP_GATTS_MTU_EVT");
            break;

        case ESP_GATTS_CONF_EVT:
            if (param->conf.status != ESP_GATT_OK) {
                ESP_LOGE(MAIN_TAG, "GATT: Notify confirm failed, status=%d (0x%x)", param->conf.status, param->conf.status);
            }
            break;

        case ESP_GATTS_RESPONSE_EVT:
            if (param->rsp.status != ESP_GATT_OK) {
                ESP_LOGE(MAIN_TAG, "GATT: response failed, status=%d (0x%x)", param->rsp.status, param->rsp.status);
            }
            break;

        default:
            ESP_LOGI(MAIN_TAG, "GATT: Unhandled GATT event: %d", event);
            break;
    }
}


//-----------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Starting app_main");

    esp_err_t ret;

    //-----------------------------------------------------------------------------
    // nvs
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(MAIN_TAG, "NVS partition needs erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    ESP_LOGI(MAIN_TAG, "NVS initialized");


    //-----------------------------------------------------------------------------
    // bt
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_LOGI(MAIN_TAG, "Initializing BT controller");
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(MAIN_TAG, "Enabling BT controller");
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(MAIN_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    //-----------------------------------------------------------------------------
    // bluedroid
    ESP_LOGI(MAIN_TAG, "Initializing Bluedroid");
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(MAIN_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(MAIN_TAG, "Enabling Bluedroid");
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(MAIN_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    //-----------------------------------------------------------------------------
    // gatt callback
    ESP_LOGI(MAIN_TAG, "Registering GATT callback");
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(MAIN_TAG, "%s gatt callback register error, error code = %s", __func__, esp_err_to_name(ret));
        return;
    }

    //-----------------------------------------------------------------------------
    // gap callback
    ESP_LOGI(MAIN_TAG, "Registering GAP callback");
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(MAIN_TAG, "%s gap callback register error, error code = %s", __func__, esp_err_to_name(ret));
        return;
    }

    //-----------------------------------------------------------------------------
    // gatt app
    ESP_LOGI(MAIN_TAG, "Registering GATT app");
    ret = esp_ble_gatts_app_register(0); // 0: app_id // triggers ESP_GATTS_REG_EVT. seq[a-1]
    if (ret) {
        ESP_LOGE(MAIN_TAG, "%s gatt app register error, error code = %s", __func__, esp_err_to_name(ret));
        return;
    }



    //-----------------------------------------------------------------------------
    // LED
    configure_led();

    //-----------------------------------------------------------------------------
    // Notify timer
    notify_timer = xTimerCreate("NotifyTimer", pdMS_TO_TICKS(30), pdTRUE, NULL, notify_timer_callback);

    //-----------------------------------------------------------------------------
    // Write timer
    write_data_read_timer = xTimerCreate("ReadTimer", pdMS_TO_TICKS(100), pdTRUE, NULL, write_data_read_timer_callback);
    if (write_data_read_timer == NULL) {
        ESP_LOGE(MAIN_TAG, "Failed to create read timer");
    } else {
        xTimerStart(write_data_read_timer, 0);
    }
    // Write data queue ( size:1, most recent only )
    write_data_queue = xQueueCreate(1, 10 * sizeof(uint8_t));
    if (write_data_queue == NULL) {
        ESP_LOGE(MAIN_TAG, "Failed to create write data queue");
    }

}

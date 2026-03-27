#include "esp_zigbee_core.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_attribute.h"
#include "zcl/esp_zigbee_zcl_power_config.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "zigbee/helpers.h"
#include "ext/motor.h"

#include "sensor.h"

const char* swBuildId = SW_VERSION;
const char* dateCode = DATE_CODE;

void ZigbeeSensor::createBasicCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    char zigbee_swid[16];
    fill_zcl_string(zigbee_swid, sizeof(zigbee_swid), swBuildId);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_SW_BUILD_ID, &zigbee_swid);

    char zigbee_datecode[50];
    fill_zcl_string(zigbee_datecode, sizeof(zigbee_datecode), dateCode);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, &zigbee_datecode);

    uint16_t stack_version = 0x30;
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_version);

    char zb_name[50];
    fill_zcl_string(zb_name, sizeof(zb_name), manufacturer_name);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &zb_name);

    char zb_model[50];
    fill_zcl_string(zb_model, sizeof(zb_model), model_identifier);
    esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &zb_model);
}


void ZigbeeSensor::createIdentifyCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createTimeCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *time_cluster_server = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_ZONE_ID, &_gmt_offset);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_ID, &_utc_time);
    esp_zb_time_cluster_add_attr(time_cluster_server, ESP_ZB_ZCL_ATTR_TIME_TIME_STATUS_ID, &_time_status);

    esp_zb_attribute_list_t *time_cluster_client = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster_server, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(cluster_list, time_cluster_client, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
}

void ZigbeeSensor::createTemperatureCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *temp_cluster = esp_zb_temperature_meas_cluster_create(&temperature_cfg);
    esp_zb_cluster_list_add_temperature_meas_cluster(cluster_list, temp_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createHumidityCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *humidity_cluster = esp_zb_humidity_meas_cluster_create(&humidity_cfg);
    esp_zb_cluster_list_add_humidity_meas_cluster(cluster_list, humidity_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createCustomClusters(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *blinds_cluster = esp_zb_zcl_attr_list_create(MS_BLIND_CLUSTER_ID);

    uint16_t val = 0;
    esp_zb_cluster_add_manufacturer_attr(
        blinds_cluster,
        MS_BLIND_CLUSTER_ID,
        ATTR_SETUP_ID,
        MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_BOOL,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
        &val
    );

    esp_zb_cluster_add_manufacturer_attr(
        blinds_cluster,
        MS_BLIND_CLUSTER_ID,
        ATTR_MIN_SPEED_ID,
        MANUFACTURER_CODE,
        ESP_ZB_ZCL_ATTR_TYPE_S32,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE,
        &val
    );

    esp_zb_cluster_list_add_custom_cluster(cluster_list, blinds_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createWindowCoveringCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *wc_cluster_server = esp_zb_window_covering_cluster_create(&wc_cluster_cfg);

    uint16_t position = 0;
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &position);
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_NUMBER_OF_ACTUATIONS_LIFT_ID, &position);
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, &position);
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, &position);
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_VELOCITY_ID, &position);
    // esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_ACCELERATION_TIME_ID, &position);
    // esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_DECELERATION_TIME_ID, &position);

    uint8_t percent = 0;
    esp_zb_window_covering_cluster_add_attr(wc_cluster_server, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &percent);

    esp_zb_cluster_list_add_window_covering_cluster(cluster_list, wc_cluster_server, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createPowerCluster(esp_zb_cluster_list_t* cluster_list) {
    uint8_t battery_percentage = 0xff;
    uint8_t battery_voltage = 0xff;

    esp_zb_attribute_list_t *power_config_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG);
    esp_zb_power_config_cluster_add_attr(power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &battery_percentage);
    // esp_zb_power_config_cluster_add_attr(power_config_cluster, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &battery_voltage);
    esp_zb_cluster_add_attr(
        power_config_cluster,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
        ESP_ZB_ZCL_ATTR_TYPE_U8,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
        &battery_voltage
    );

    esp_zb_cluster_list_add_power_config_cluster(cluster_list, power_config_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
}

void ZigbeeSensor::createOtaCluster(esp_zb_cluster_list_t* cluster_list) {
    esp_zb_attribute_list_t *ota_cluster = esp_zb_ota_cluster_create(&ota_cluster_cfg);

    esp_zb_zcl_ota_upgrade_client_variable_t variable_config = {};
    variable_config.timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF;
    variable_config.hw_version = 3;
    variable_config.max_data_size = 223;

    uint16_t ota_upgrade_server_addr = 0xffff;
    uint8_t ota_upgrade_server_ep = 0xff;

    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &variable_config);
    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, &ota_upgrade_server_addr);
    esp_zb_ota_cluster_add_attr(ota_cluster, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, &ota_upgrade_server_ep);

    esp_zb_cluster_list_add_ota_cluster(cluster_list, ota_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
}

static void findOTAServer(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        esp_zb_ota_upgrade_client_query_interval_set(*((uint8_t *)user_ctx), OTA_UPGRADE_QUERY_INTERVAL);
        esp_zb_ota_upgrade_client_query_image_req(addr, endpoint);
        ESP_LOGI("FIND_OTA", "Query OTA upgrade from server endpoint: %d after %d seconds", endpoint, OTA_UPGRADE_QUERY_INTERVAL);
    } else {
        ESP_LOGW("FIND_OTA", "No OTA Server found");
    }
}

void ZigbeeSensor::requestOTA() {
    esp_zb_zdo_match_desc_req_param_t req;
    uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE};

    req.addr_of_interest = 0x0000;
    req.dst_nwk_addr = 0x0000;
    req.num_in_clusters = 1;
    req.num_out_clusters = 0;
    req.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    req.cluster_list = cluster_list;
    esp_zb_lock_acquire(portMAX_DELAY);
    if (esp_zb_bdb_dev_joined()) {
        esp_zb_zdo_match_cluster(&req, findOTAServer, &_endpoint);
    }
    esp_zb_lock_release();
}

esp_zb_cluster_list_t* ZigbeeSensor::createClusters() {
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    createBasicCluster(cluster_list);
    createIdentifyCluster(cluster_list);
    createPowerCluster(cluster_list);
    createOtaCluster(cluster_list);
    createTimeCluster(cluster_list);
    createCustomClusters(cluster_list);
    createWindowCoveringCluster(cluster_list);
    createTemperatureCluster(cluster_list);
    createHumidityCluster(cluster_list);

    return cluster_list;
}

void ZigbeeSensor::setBattery(uint8_t battery, uint8_t percentage) {
    esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
        &percentage,
        false
    );

    esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
        &battery,
        false
    );
}

void ZigbeeSensor::init(Preferences* prefs) {
    _prefs = prefs;
    velocity = _prefs->getUShort(NVS_VELOCITY, INT16_MAX);
    min = _prefs->getULong64(NVS_MIN, 0);
    uint64_t lmax = _prefs->getULong64(NVS_MAX, UINT64_MAX);
    minSpeed = _prefs->getInt(NVS_MIN_SPEED, 22000);
    max = (lmax - min) / (1 << 4);

    motor.setVelocity(velocity);
    motor.setEnds(min, lmax);
    motor.setMinSpeed(minSpeed);
}

void ZigbeeSensor::onConnect() {
    esp_zb_lock_acquire(portMAX_DELAY);
    void* varArr[] = {
        &velocity, &max, &minSpeed
    };
    uint16_t attrIdArr[] = {
        ESP_ZB_ZCL_ATTR_WINDOW_COVERING_VELOCITY_ID, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID,
        ATTR_MIN_SPEED_ID
    };
    uint16_t clusterIdArr[] = {
        ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
        MS_BLIND_CLUSTER_ID
    };
    uint8_t items = sizeof(attrIdArr) / sizeof(uint16_t);

    for (uint8_t i = 0; i < items; i++) {
        if (clusterIdArr[i] >= 0xFC00) {
            esp_zb_zcl_set_manufacturer_attribute_val(
                _endpoint,
                clusterIdArr[i],
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                MANUFACTURER_CODE,
                attrIdArr[i],
                varArr[i],
                false
            );
        } else {
            esp_zb_zcl_set_attribute_val(
                _endpoint,
                clusterIdArr[i],
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                attrIdArr[i],
                varArr[i],
                false
            );
        }
    }
    esp_zb_lock_release();
}

void ZigbeeSensor::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY) {
        uint16_t state = *(uint16_t *)message->attribute.data.value;
        gpio_hold_dis(LED_PIN);
        gpio_set_level(LED_PIN, state % 2);
        if (state > 0) gpio_hold_en(LED_PIN);
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
        switch (message->attribute.id) {
            case ESP_ZB_ZCL_ATTR_WINDOW_COVERING_VELOCITY_ID:
                velocity = *(uint16_t *)message->attribute.data.value;
                _prefs->putUShort(NVS_VELOCITY, velocity);
                motor.setVelocity(velocity);
                break;
        }
    } else if (message->info.cluster == MS_BLIND_CLUSTER_ID) {
        switch (message->attribute.id) {
            case ATTR_SETUP_ID: {
                bool setup = *(bool *)message->attribute.data.value;
                motor.setSetup(setup);
                break;
            }
            case ATTR_MIN_SPEED_ID: {
                minSpeed = *(int32_t *)message->attribute.data.value;
                _prefs->putInt(NVS_MIN_SPEED, minSpeed);
                motor.setMinSpeed(minSpeed);
                break;
            }
        }
    }
}

void ZigbeeSensor::zbCommand(const zb_zcl_parsed_hdr_t* cmdInfo, const void* data) {
    if (cmdInfo->cluster_id == MS_BLIND_CLUSTER_ID) {
        uint64_t temp;
        switch (cmdInfo->cmd_id) {
            case CMD_SET_MIN_ID:
                min = motor.setMin();
                _prefs->putULong64(NVS_MIN, min);
                break;
            case CMD_SET_MAX_ID:
                temp = motor.setMax();
                _prefs->putULong64(NVS_MAX, temp);
                max = (temp - min) / (1 << 4);

                esp_zb_zcl_set_attribute_val(
                    _endpoint,
                    ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
                    ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                    ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID,
                    &max,
                    false
                );
                break;
            case CMD_NUDGE_ID:
                motor.nudge(*(int16_t *)data);
                break;
        }
    } else if (cmdInfo->cluster_id == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
        switch (cmdInfo->cmd_id) {
            case ESP_ZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN:
                // Up
                return motor.goDirection(true);
            case ESP_ZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE:
                // Down
                return motor.goDirection(false);
            case ESP_ZB_ZCL_CMD_WINDOW_COVERING_STOP:
                // Stop
                return motor.stop();
            case ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_VALUE: {
                // Go to position
                uint16_t position = *(uint16_t *)data;
                return motor.goPosition(min + (position * (1 << 4)));
            }
            case ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE: {
                // Go to percentage
                uint8_t percent = *(uint8_t *)data;
                return motor.goPercent(percent);
            }
        }
    }
}

ZigbeeSensor::ZigbeeSensor(uint8_t endpoint) : ZigbeeDevice(ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, endpoint) {    
    basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_BATTERY
    };
    identify_cfg = {
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE
    };
    ota_cluster_cfg = {
        .ota_upgrade_file_version = FW_VERSION,
        .ota_upgrade_manufacturer = 0x1001,
        .ota_upgrade_image_type = 0x1014,
        .ota_min_block_reque = 0,
        .ota_upgrade_file_offset = 0,
        .ota_upgrade_downloaded_file_ver = ESP_ZB_ZCL_OTA_UPGRADE_DOWNLOADED_FILE_VERSION_DEF_VALUE,
        .ota_upgrade_server_id = 0,
        .ota_image_upgrade_status = 0
    };
    wc_cluster_cfg = {
        .covering_type = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE,
        .covering_status = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_OPERATIONAL | ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_ONLINE |
            ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_CONTROL_IS_CLOSED_LOOP | ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_ENCODER_CONTROLLED,
        .covering_mode = 0
    };
    temperature_cfg = {
        .measured_value = (int16_t) 0x8000,
        .min_value = -5000,
        .max_value = 10000
    };
    humidity_cfg = {
        .measured_value = (uint16_t) 0xffff,
        .min_value = 0,
        .max_value = 10000
    };

    _cluster_list = createClusters();
}

tm ZigbeeSensor::fetchTime() {
    time_t unixTime;
    time(&unixTime);

    // Unix time is large enough to be correct and has been updated in the last day
    if (unixTime > 86400 * 30 && esp_timer_get_time() - lastTime <= 86400000000) {
        tm time;
        localtime_r(&unixTime, &time);
        return time;
    }

    return getTime(_endpoint);
}

bool ZigbeeSensor::setTemperature(float temperature) {
    int16_t zigbeeTemp = temperature * 100;

    esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;

    esp_zb_lock_acquire(portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        &zigbeeTemp,
        false
    );
    esp_zb_lock_release();

    bool res = ret == ESP_ZB_ZCL_STATUS_SUCCESS;
    if (!res) {
        ESP_LOGE(TAG, "Failed to set temperature: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    }
    return res;
}

bool ZigbeeSensor::setHumidity(float humidity) {
    int16_t zigbeeHumidity = humidity * 100;

    esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;

    esp_zb_lock_acquire(portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
        &zigbeeHumidity,
        false
    );
    esp_zb_lock_release();

    bool res = ret == ESP_ZB_ZCL_STATUS_SUCCESS;
    if (!res) {
        ESP_LOGE(TAG, "Failed to set humidity: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    }
    return res;
}

bool ZigbeeSensor::setBlindState(uint8_t percent, uint16_t position, uint16_t actuations) {
    esp_zb_zcl_status_t ret, ret2, ret3 = ESP_ZB_ZCL_STATUS_SUCCESS;

    esp_zb_lock_acquire(portMAX_DELAY);
    ret = esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
        &percent,
        false
    );

    ret2 = esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID,
        &position,
        false
    );

    ret3 = esp_zb_zcl_set_attribute_val(
        _endpoint,
        ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_WINDOW_COVERING_NUMBER_OF_ACTUATIONS_LIFT_ID,
        &actuations,
        false
    );
    esp_zb_lock_release();

    bool res = (ret | ret2 | ret3) == ESP_ZB_ZCL_STATUS_SUCCESS;
    if (!res) {
        ESP_LOGE(TAG, "Failed to set percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    }
    return res;
}

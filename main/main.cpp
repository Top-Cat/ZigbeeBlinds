#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "nvs_flash.h"

#include "config.h"
#include "sensor.h"
#include "ext/adc.h"
#include "ext/hdc2080.h"
#include "ext/motor.h"
#include "zigbee/handlers.h"
#include "zigbee/core.h"

////////////////////////

static const char *TAG = "TC-ZB";

static QueueHandle_t main_task_queue;
volatile bool button_pressed = false;

uint64_t lastHeartbeat = 0;
uint16_t heartbeatCounter = 0;

ZigbeeSensor zbEndpoint = ZigbeeSensor(ENDPOINT_NUMBER);

////////////////////////

void IRAM_ATTR buttonISR(void* data) {
    button_pressed = true;

    BaseType_t hpw = pdFALSE;
    uint8_t dummy = 0;
    xQueueSendFromISR(main_task_queue, &dummy, &hpw);
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;   
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
    esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
    esp_zb_zdo_signal_nwk_status_indication_params_s* nlme_params = NULL;
    uint8_t dummy = 0;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Device started up in %sfactory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non-");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
                zigbeeCore.started = true;
            } else {
                ESP_LOGI(TAG, "Device rebooted");
                zigbeeCore.started = true;
                zigbeeCore.connected = true;
                zigbeeCore.setChannelMask(1 << esp_zb_get_current_channel());
                zigbeeCore.searchBindings();
            }
        } else {
            // commissioning failed
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION, 500);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            zigbeeCore.connected = true;
            zigbeeCore.setChannelMask(1 << esp_zb_get_current_channel());
        } else {
            ESP_LOGV(TAG, "Network steering was not successful (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        esp_zb_sleep_now();
        xQueueSend(main_task_queue, &dummy, 0);
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
        leave_params = (esp_zb_zdo_signal_leave_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGV(TAG, "Signal to leave the network, leave type: %d", leave_params->leave_type);
        if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
            ESP_LOGI(TAG, "Leave without rejoin, factory reset the device");
            esp_zb_factory_reset();
        } else { // Leave with rejoin -> Rejoin the network, only reboot the device
            ESP_LOGI(TAG, "Leave with rejoin, only reboot the device");
            esp_restart();
        }
        break;
    case ESP_ZB_NLME_STATUS_INDICATION:
        nlme_params = (esp_zb_zdo_signal_nwk_status_indication_params_s *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGV(TAG, "NLME status indication: %02x 0x%04x %02x", nlme_params->status, nlme_params->network_addr, nlme_params->unknown_command_id);
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

void handleResetButton() {
    if (!button_pressed)
        return;

    button_pressed = false;

    uint64_t pressStart = esp_timer_get_time();
    while (gpio_get_level(EXT_BUTTON_PIN) == 0) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if (esp_timer_get_time() - pressStart > 3000000) {
            ESP_LOGW(TAG, "Resetting Zigbee to factory and rebooting in 1s.");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_zb_factory_reset();
        }
    }
}

void handleHeartbeat() {
    if (esp_timer_get_time() - lastHeartbeat <= HEARTBEAT_INTERVAL)
        return;

    lastHeartbeat = esp_timer_get_time();

    if (heartbeatCounter % 60 == 0) {
        // Every hour
        uint8_t zigbeeMv, zigbeePercent;
        if (adc.getValue(zigbeeMv, zigbeePercent)) {
            ESP_LOGI(TAG, "ADC result = %d, %d", zigbeeMv, zigbeePercent);
            zbEndpoint.setBattery(zigbeeMv, zigbeePercent);
        }

        // HDC2080 will automatically update
        zbEndpoint.setTemperature(hdc2080.getTemp());
        zbEndpoint.setHumidity(hdc2080.getHumidity());
    }

    heartbeatCounter++;

    if (!zigbeeCore.connected) {
        ESP_LOGI(TAG, "Zigbee not connected, attempting reconnect...");
        zigbeeCore.start();
    }
}

static void mainTask(void *pvParameters) {
    uint8_t local;
    while (true) {
        xQueueReceive(main_task_queue, &local, portMAX_DELAY);

        handleResetButton();
        handleHeartbeat();
    }
}

static esp_err_t powerSaveInit() {
    int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = cur_cpu_freq_mhz,
        .min_freq_mhz = cur_cpu_freq_mhz,
        .light_sleep_enable = true
    };
    return esp_pm_configure(&pm_config);
}

void motorMove(const uint8_t percent, const uint16_t position, const uint16_t actuations) {
    zbEndpoint.setBlindState(percent, position, actuations);
}

Preferences prefs;

extern "C" void app_main(void) {
    powerSaveInit();
    main_task_queue = xQueueCreate(4, sizeof(uint8_t));

    gpio_config_t gpioConfig = {
        .pin_bit_mask = BIT64(BUTTON_PIN) | BIT64(EXT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&gpioConfig);

    gpioConfig.pin_bit_mask = BIT64(LED_PIN) | BIT64(BAT_LOW_PIN);
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&gpioConfig);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(EXT_BUTTON_PIN, buttonISR, NULL);

    ESP_ERROR_CHECK(nvs_flash_init());
    prefs.begin(NVS_NAMESPACE, false);
    hdc2080.init();

    uint8_t zigbeeMv, zigbeePercent;
    adc.getValue(zigbeeMv, zigbeePercent);
    if (zigbeeMv < 60) {
        // On USB power disable motor!
        ESP_LOGW(TAG, "On USB power. Skipping motor init");
    } else {
        motor.init(&prefs);
    }

    // printf("Init complete\n");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    motor.identify();

    zbEndpoint.init(&prefs);

    zigbeeCore.registerEndpoint(&zbEndpoint);
    zigbeeCore.start(zbEndpoint.getKeepAlive());

    while (!zigbeeCore.connected) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    zbEndpoint.onConnect();
    zbEndpoint.setBattery(zigbeeMv, zigbeePercent);
    zbEndpoint.requestOTA();
    zbEndpoint.fetchTime();
    motor.moveCallback(motorMove);

    xTaskCreate(mainTask, "Main", 4096, NULL, 4, NULL);
}

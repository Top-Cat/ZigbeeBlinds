#include "esp_zigbee_type.h"

#include "zigbee/endpoint.h"
#include "prefs.h"

#define MANUFACTURER_CODE        0x1234

#define MS_BLIND_CLUSTER_ID      0xFC13
#define ATTR_SETUP_ID            0x01
#define CMD_SET_MIN_ID           0xF1
#define CMD_SET_MAX_ID           0xF2
#define CMD_NUDGE_ID             0xF3

#define OTA_UPGRADE_QUERY_INTERVAL (1 * 60)
#define NVS_NAMESPACE         "config"
#define NVS_VELOCITY          "velocity"
#define NVS_MIN               "min"
#define NVS_MAX               "max"

class ZigbeeSensor : public ZigbeeDevice {
    public:
        ZigbeeSensor(uint8_t endpoint);
        ~ZigbeeSensor() {};

        void zbCommand(const zb_zcl_parsed_hdr_t* cmdInfo, const void* data) override;
        void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

        void init(Preferences* prefs);
        void setBattery(uint8_t battery, uint8_t percentage);
        bool setTemperature(float temp);
        bool setHumidity(float humidity);
        bool setBlindState(uint8_t percent, uint16_t position, uint16_t actuations);

        void onConnect();
        void requestOTA();

        tm fetchTime();
    private:
        const char* TAG = "TC-ZBS";
        const char* manufacturer_name = "TC";
        const char* model_identifier = "Blinds";

        time_t _utc_time = 0;
        int32_t _gmt_offset = 0;
        uint64_t lastTime = 0;

        uint16_t velocity = 0;
        uint64_t min = 0;
        uint16_t max = 0;

        Preferences* _prefs;

        esp_zb_basic_cluster_cfg_t basic_cfg;
        esp_zb_identify_cluster_cfg_t identify_cfg;
        esp_zb_ota_cluster_cfg_t ota_cluster_cfg;
        esp_zb_window_covering_cluster_cfg_t wc_cluster_cfg;
        esp_zb_temperature_meas_cluster_cfg_t temperature_cfg;
        esp_zb_humidity_meas_cluster_cfg_t humidity_cfg;

        esp_zb_cluster_list_t* createClusters() override;
        void createBasicCluster(esp_zb_cluster_list_t* cluster_list);
        void createIdentifyCluster(esp_zb_cluster_list_t* cluster_list);
        void createPowerCluster(esp_zb_cluster_list_t* cluster_list);
        void createOtaCluster(esp_zb_cluster_list_t* cluster_list);
        void createTimeCluster(esp_zb_cluster_list_t* cluster_list);
        void createTemperatureCluster(esp_zb_cluster_list_t* cluster_list);
        void createHumidityCluster(esp_zb_cluster_list_t* cluster_list);
        void createWindowCoveringCluster(esp_zb_cluster_list_t* cluster_list);
        void createCustomClusters(esp_zb_cluster_list_t* cluster_list);
};

#include "../config.h"

#include "i2cdev.h"

typedef enum {
    TEMP_L = 0x00,
    TEMP_H = 0x01,
    HUMIDITY_L = 0x02,
    HUMIDITY_H = 0x03,
    INT_DRDY = 0x04,
    TEMP_MAX = 0x05,
    HUMIDITY_MAX = 0x06,
    INT_EN = 0x07,
    TEMP_OFFSET = 0x08,
    HUMIDITY_OFFSET = 0x09,
    TEMP_L_TRESH = 0x0A,
    TEMP_H_TRESH = 0x0B,
    HUMIDITY_L_TRESH = 0x0C,
    HUMIDITY_H_TRESH = 0x0D,
    CONFIGURATION = 0x0E,
    MEASUREMENT_CFG = 0x0F,
    MAN_ID_L = 0xFC,
    MAN_ID_H = 0xFD,
    DEV_ID_L = 0xFE,
    DEV_ID_H = 0xFF
} HCmd;

typedef enum {
    INT_DATA_READY = 0x80,
    INT_TEMP_HIGH = 0x40,
    INT_TEMP_LOW = 0x20,
    INT_HUMID_HIGH = 0x10,
    INT_HUMID_LOW = 0x08,
} HIntCfg;

typedef enum {
    INT_MODE_LEVEL_SENSITIVE = 0,
    INT_MODE_COMPARATOR = 1
} HIntMode;

typedef enum {
    INT_HIGH_Z = 0,
    INT_ENABLE = 1
} HIntEn;

typedef enum {
    INT_POLARITY_ACTIVE_LOW = 0,
    INT_POLARITY_ACTIVE_HIGH = 1
} HIntPolarity;

typedef enum {
    HEATER_OFF = 0,
    HEATER_ON = 1
} HHeater;

typedef enum {
    AUTO_MODE_DISABLED = 0,
    AUTO_MODE_2_MINUTES = 1,
    AUTO_MODE_1_MINUTES = 2,
    AUTO_MODE_10_SECONDS = 3,
    AUTO_MODE_5_SECONDS = 4,
    AUTO_MODE_1_SECONDS = 5,
    AUTO_MODE_0_5_SECONDS = 6,
    AUTO_MODE_0_2_SECONDS = 7
} HAutoMode;

typedef enum {
    RESET_DISABLED = 0,
    RESET_ENABLED = 1
} HReset;

typedef enum {
    MEASURE_TEMP_HUMIDITY = 0,
    MEASURE_TEMP = 1
} HMeasSrc;

typedef enum {
    RESOLUTION_14_BIT = 0,
    RESOLUTION_11_BIT = 1,
    RESOLUTION_9_BIT = 2
} HResolution;

struct HCfg {
    HIntMode interuptMode : 1;
    HIntPolarity polarity : 1;
    HIntEn enabled : 1;
    HHeater heater : 1;
    HAutoMode autoMode : 3;
    HReset reset : 1;
} __attribute__((packed));

struct HMeasCfg {
    bool trigger : 1;
    HMeasSrc source : 2;
    bool reserved : 1;
    HResolution humidRes : 2;
    HResolution tempRes : 2;
} __attribute__((packed));

class HDC2080 {
    private:
        const uint16_t addr = 0x40;
        uint64_t lastUpdate = 0;
        i2c_dev_t i2c;

        uint16_t rawTemperature = 0;
        uint16_t rawHumidity = 0;
        uint8_t maxTemperature = 0;
        uint8_t maxHumidity = 0;

        esp_err_t initDevice();
        esp_err_t update();
    public:
        void init();
        float getTemp();
        float getHumidity();

        esp_err_t setTempOffset(float offset);
        esp_err_t setTempThresholdLow(const float temp);
        esp_err_t setTempThresholdHigh(const float temp);

        esp_err_t setHumidityOffset(float offset);
        esp_err_t setHumidityThresholdLow(const float perc);
        esp_err_t setHumidityThresholdHigh(const float perc);

        esp_err_t getManufacturerId(uint16_t* out);
        esp_err_t getDeviceId(uint16_t* out);
};

extern HDC2080 hdc2080;

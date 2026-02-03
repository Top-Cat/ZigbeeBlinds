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
} HDC2080_REG;

class HDC2080 {
    private:
        const uint16_t addr = 0x40;
        uint64_t lastUpdate = 0;
        i2c_dev_t i2c;

        uint16_t rawTemperature = 0;
        uint16_t rawHumidity = 0;

        esp_err_t initDevice();
        esp_err_t readValues();

        esp_err_t update();
    public:
        void init();
        float getTemp();
        float getHumidity();
};

extern HDC2080 hdc2080;

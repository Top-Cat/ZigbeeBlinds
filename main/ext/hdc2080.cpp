#include "hdc2080.h"

#include "esp_timer.h"

HDC2080 hdc2080;

SemaphoreHandle_t lock;
void IRAM_ATTR dataReadyISR(void* data) {
    xSemaphoreGive(lock);
}

void HDC2080::initMutex() {
    i2c.port = I2C_NUM_0;
    i2c.addr = addr;
    i2c.cfg.master.clk_speed = 400000;
    i2c.cfg.sda_io_num = TEMP_SDA_PIN;
    i2c.cfg.scl_io_num = TEMP_SCL_PIN;
    i2c_dev_create_mutex(&i2c);
}

void HDC2080::init() {
    if (!lock) {
        lock = xSemaphoreCreateBinary();
    }

    gpio_config_t gpioConfig = {
        .pin_bit_mask = BIT64(TEMP_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&gpioConfig);

    ESP_ERROR_CHECK(i2cdev_init());

    initMutex();
    initDevice();
    gpio_isr_handler_add(TEMP_INT_PIN, dataReadyISR, NULL);
}

esp_err_t HDC2080::initDevice() {
    I2C_DEV_TAKE_MUTEX(&i2c);

    const HCfg cfg = {
        .interuptMode = INT_MODE_LEVEL_SENSITIVE,
        .polarity = INT_POLARITY_ACTIVE_HIGH,
        .enabled = INT_ENABLE,
        .heater = HEATER_OFF,
        .autoMode = AUTO_MODE_DISABLED,
        .reset = RESET_DISABLED
    };
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::CONFIGURATION, &cfg, sizeof(cfg)));

    const uint8_t intCfg = HIntCfg::INT_DATA_READY;
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::INT_EN, &intCfg, sizeof(intCfg)));

    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::update() {
    // If we updated recently and we have data, skip update
    if (lastUpdate + (1 * 1000 * 1000) > esp_timer_get_time() && rawTemperature > 0 && rawHumidity > 0) return ESP_OK;
    lastUpdate = esp_timer_get_time();

    i2cdev_init();
    initMutex();

    const HMeasCfg meas = {
        .trigger = true,
        .source = MEASURE_TEMP_HUMIDITY,
        .reserved = false,
        .humidRes = RESOLUTION_14_BIT,
        .tempRes = RESOLUTION_14_BIT,
    };

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::MEASUREMENT_CFG, &meas, sizeof(meas)));

    // Wait for data to be ready
    if (xSemaphoreTake(lock, 1000 / portTICK_PERIOD_MS) != pdTRUE) {
        // Failed
        I2C_DEV_GIVE_MUTEX(&i2c);
        return ESP_FAIL;
    }

    uint8_t raw_data[7];
    I2C_DEV_CHECK(&i2c, i2c_dev_read_reg(&i2c, HCmd::TEMP_L, &raw_data, sizeof(raw_data)));
    rawTemperature = raw_data[0] | (raw_data[1] << 8);
    rawHumidity = raw_data[2] | (raw_data[3] << 8);

    maxTemperature = raw_data[5];
    maxHumidity = raw_data[6];

    I2C_DEV_GIVE_MUTEX(&i2c);

    i2c_dev_delete_mutex(&i2c);
    i2cdev_done();

    return ESP_OK;
}

float HDC2080::getTemp() {
    update();
    return rawTemperature * 0.0025177f - 40.5f;
}

float HDC2080::getHumidity() {
    update();
    return rawHumidity * 0.001525879f;
}

esp_err_t HDC2080::setTempOffset(float offset) {
    uint8_t reg = 0;
    float lowOffset = -20.62f;

    if (offset < lowOffset || offset > 20.46f) {
        // Impossible offset
        return ESP_ERR_INVALID_ARG;
    } else if (offset < 0) {
        reg |= 0x80;
        offset -= lowOffset;
    }

    reg |= (uint8_t) (offset / 0.16f);

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::TEMP_OFFSET, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::setTempThresholdLow(const float temp) {
    uint8_t reg = (temp * 0.64453125f) - 40.5f;

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::TEMP_L_TRESH, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::setTempThresholdHigh(const float temp) {
    uint8_t reg = (temp * 0.64453125f) - 40.5f;

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::TEMP_H_TRESH, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::setHumidityOffset(float offset) {
    uint8_t reg = 0;
    float lowOffset = -25.0f;

    if (offset < lowOffset || offset > 24.8f) {
        // Impossible offset
        return ESP_ERR_INVALID_ARG;
    } else if (offset < 0) {
        reg |= 0x80;
        offset -= lowOffset;
    }

    reg |= (uint8_t) (offset / 0.1953125f);

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::HUMIDITY_OFFSET, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::setHumidityThresholdLow(const float perc) {
    uint8_t reg = perc * 0.390625f;

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::HUMIDITY_L_TRESH, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::setHumidityThresholdHigh(const float perc) {
    uint8_t reg = perc * 0.390625f;

    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HCmd::HUMIDITY_H_TRESH, &reg, sizeof(reg)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::getManufacturerId(uint16_t* out) {
    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_read_reg(&i2c, HCmd::MAN_ID_L, out, sizeof(*out)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::getDeviceId(uint16_t* out) {
    I2C_DEV_TAKE_MUTEX(&i2c);
    I2C_DEV_CHECK(&i2c, i2c_dev_read_reg(&i2c, HCmd::DEV_ID_L, out, sizeof(*out)));
    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

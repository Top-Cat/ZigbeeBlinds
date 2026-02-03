#include "hdc2080.h"

#include "esp_timer.h"

HDC2080 hdc2080;

SemaphoreHandle_t lock;
void IRAM_ATTR dataReadyISR(void* data) {
    xSemaphoreGive(lock);
}

void HDC2080::init() {
    if (!lock) {
        lock = xSemaphoreCreateBinary();
    }

    gpio_config_t gpioConfig = {
        .pin_bit_mask = BIT(TEMP_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_HIGH_LEVEL
    };
    gpio_config(&gpioConfig);
    gpio_isr_handler_add(TEMP_INT_PIN, dataReadyISR, NULL);

    ESP_ERROR_CHECK(i2cdev_init());

    i2c.port = I2C_NUM_0;
    i2c.addr = addr;
    i2c.cfg.sda_io_num = TEMP_SDA_PIN;
    i2c.cfg.scl_io_num = TEMP_SCL_PIN;
    i2c_dev_create_mutex(&i2c);

    initDevice();
}

esp_err_t HDC2080::initDevice() {
    I2C_DEV_TAKE_MUTEX(&i2c);

    const uint8_t cfg = 0b00000110;  // automatic measurement mode disabled, heater off
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HDC2080_REG::CONFIGURATION, &cfg, sizeof(cfg)));

    const uint8_t intCfg = 0b10000000;  // data ready interupt
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HDC2080_REG::INT_EN, &intCfg, sizeof(intCfg)));

    I2C_DEV_GIVE_MUTEX(&i2c);

    return ESP_OK;
}

esp_err_t HDC2080::update() {
    // If we updated recently and we have data, skip update
    if (lastUpdate + (60 * 1000 * 1000) > esp_timer_get_time() && rawTemperature > 0 && rawHumidity > 0) return ESP_OK;

    I2C_DEV_TAKE_MUTEX(&i2c);

    const uint8_t data = 0b00000001;  // resolution 14bit, sample both humidity and temperature, start measurement
    I2C_DEV_CHECK(&i2c, i2c_dev_write_reg(&i2c, HDC2080_REG::MEASUREMENT_CFG, &data, sizeof(data)));

    // Wait for data to be ready
    if (xSemaphoreTake(lock, 1000 / portTICK_PERIOD_MS) != pdTRUE) {
        // Failed
        I2C_DEV_GIVE_MUTEX(&i2c);
        return ESP_FAIL;
    }

    // Read and clear interupt flags
    uint8_t intrStatus = 0;
    I2C_DEV_CHECK(&i2c, i2c_dev_read_reg(&i2c, HDC2080_REG::INT_DRDY, &intrStatus, sizeof(intrStatus)));

    uint8_t raw_data[4];
    I2C_DEV_CHECK(&i2c, i2c_dev_read_reg(&i2c, HDC2080_REG::TEMP_L, &raw_data, sizeof(raw_data)));
    rawTemperature = raw_data[0] | (raw_data[1] << 8);
    rawHumidity = raw_data[2] | (raw_data[3] << 8);

    I2C_DEV_GIVE_MUTEX(&i2c);

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

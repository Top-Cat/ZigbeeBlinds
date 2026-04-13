#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adc.h"

BSADC adc;

void BSADC::init() {
    channel = (adc_channel_t) BAT_ADC_PIN;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = (adc_oneshot_clk_src_t) 0,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, channel, &config));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle));
}

void BSADC::enable() {
    init();
    gpio_set_direction(BAT_LOW_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BAT_LOW_PIN, 0);
}

void BSADC::disable() {
    gpio_set_direction(BAT_LOW_PIN, GPIO_MODE_INPUT);
    adc_oneshot_del_unit(adc_handle);
}

bool BSADC::getValue(uint8_t &out, uint8_t &percent, uint16_t &precise) {
    enable();

    vTaskDelay(100 / portTICK_PERIOD_MS);

    int adcValue;
    bool res = getAdcValue(adcValue);
    if (res) {
        precise = adcValue * multiplier;
        out = (uint8_t) ((precise + 50) / 100); // + 50 makes rounding correct
        percent = (uint8_t) ((precise - BAT_LOW_MV) / percentMult);
    }

    disable();

    return res;
}

bool BSADC::getAdcValue(int &result, const uint8_t samples) {
    int adcValue, sum = 0;
    uint8_t realSamples = 0;
    for (uint8_t i = 0; i < samples; i++) {
        bool res = adc_oneshot_get_calibrated_result(adc_handle, cali_handle, channel, &adcValue) == ESP_OK;
        if (res) {
            realSamples++;
            sum += adcValue;
        }
    }

    if (realSamples == 0) {
        result = 0;
        return false;
    }

    result = sum / realSamples;
    return true;
}

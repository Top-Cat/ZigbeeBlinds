#include "../config.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#define R1_K 2000
#define R2_K 470
#define BAT_HIGH_MV 12500
#define BAT_LOW_MV 8800

class BSADC {
    public:
        void init();
        bool getValue(uint8_t &out, uint8_t &percent, uint16_t &precise);
    private:
        adc_oneshot_unit_handle_t adc_handle;
        adc_cali_handle_t cali_handle;
        adc_channel_t channel;

        const float multiplier = (R1_K + R2_K) / (R2_K * 1.0);
        const float percentMult = (BAT_HIGH_MV - BAT_LOW_MV) / 200.0;

        void enable();
        void disable();
        bool getAdcValue(int &result, const uint8_t samples = 5);
};

extern BSADC adc;

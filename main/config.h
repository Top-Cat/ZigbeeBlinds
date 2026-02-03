#include "driver/gpio.h"

#define DO_EXPAND(VAL)  VAL ## 1
#define EXPAND(VAL)     DO_EXPAND(VAL)
#define CI              (EXPAND(FW_VERSION) != 1)

#if !defined(DATE_CODE) || !CI
#undef DATE_CODE
#define DATE_CODE "19700101"
#endif

#if !defined(SW_VERSION) || !CI
#undef SW_VERSION
#define SW_VERSION "0.0.0"
#endif

#if !defined(FW_VERSION) || !CI
#undef FW_VERSION
#define FW_VERSION 0x00000001
#endif

#define ENDPOINT_NUMBER 1
#define HEARTBEAT_INTERVAL 60000000

#define ENCODER_A_PIN GPIO_NUM_4
#define ENCODER_B_PIN GPIO_NUM_5
#define MOTOR_A_PIN GPIO_NUM_14
#define MOTOR_B_PIN GPIO_NUM_15
#define EXT_BUTTON_PIN  GPIO_NUM_2
#define BUTTON_PIN  GPIO_NUM_9

#define BAT_LOW_PIN GPIO_NUM_18
#define BAT_ADC_PIN GPIO_NUM_19
#define TEMP_SDA_PIN GPIO_NUM_6
#define TEMP_SCL_PIN GPIO_NUM_0
#define TEMP_INT_PIN GPIO_NUM_1

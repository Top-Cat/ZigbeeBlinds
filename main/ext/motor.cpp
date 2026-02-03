#include "motor.h"

#include "freertos/FreeRTOS.h"
#include "driver/pulse_cnt.h"
#include "driver/ledc.h"

QueueHandle_t motorQueue;
BlindMotor motor;

static bool pcnt_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

static void motor_task(void *pvParameters) {
    motor.task();
}

void BlindMotor::motorPwmSetup() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 4000,
        .clk_cfg          = LEDC_AUTO_CLK,
        .deconfigure      = false
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num       = MOTOR_A_PIN,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,
        .hpoint         = 0,
        .sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {
            .output_invert = false
        }
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.gpio_num = MOTOR_B_PIN;
    ledc_channel.channel = LEDC_CHANNEL_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void BlindMotor::feedbackSetup() {
    pcnt_unit_config_t unit_config = {
        .low_limit = -100,
        .high_limit = 100,
        .intr_priority = 0,
        .flags = {
            .accum_count = 0
        }
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_A_PIN,
        .level_gpio_num = ENCODER_B_PIN,
        .flags = {
            .invert_edge_input = false,
            .invert_level_input = false,
            .virt_edge_io_level = false,
            .virt_level_io_level = false,
            .io_loop_back = false
        }
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_B_PIN,
        .level_gpio_num = ENCODER_A_PIN,
        .flags = {
            .invert_edge_input = false,
            .invert_level_input = false,
            .virt_edge_io_level = false,
            .virt_level_io_level = false,
            .io_loop_back = false
        }
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_cb,
    };
    motorQueue = xQueueCreate(10, sizeof(int));
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, motorQueue));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

void BlindMotor::updateSpeed(int16_t speed) {
    bool forward = speed > 0;
    uint16_t duty = abs(speed) / 4;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, forward ? duty : 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, forward ? 0 : duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

void BlindMotor::init() {
    motorPwmSetup();
    feedbackSetup();

    xTaskCreate(motor_task, "Motor", 4096, NULL, 4, NULL);

    updateSpeed(5000);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    updateSpeed(-5000);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    updateSpeed(0);
}

void BlindMotor::task() {
    int eventCount = 0;
    while (true) {
        // TODO: Also receive and handle go to position / percent
        xQueueReceive(motorQueue, &eventCount, portMAX_DELAY);

        _position += eventCount;
        printf("Watch point event, count: %d, result: %llu\n", eventCount, _position);
    }
}

void BlindMotor::goPosition(uint64_t p) {
    if (p < _min || p > _max) return;
    _target = p;
}

void BlindMotor::goPercent(uint8_t p) {
    if (p > 100) return;
    _target = _min + ((p / 100.0) * (_max - _min));
}

void BlindMotor::stop() {
    _target = _position;
}

void BlindMotor::setMin() {
    _min = _position;
}

void BlindMotor::setMax() {
    _max = _position;
}

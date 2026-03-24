#include "motor.h"

#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"

QueueHandle_t motorQueue;
BlindMotor motor;

static bool pcnt_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;

    pcnt_unit_clear_count(unit);
    xQueueSendFromISR(queue, &(edata->watch_point_value), &high_task_wakeup);
    return (high_task_wakeup == pdTRUE);
}

static void motor_task(void *pvParameters) {
    motor.task();
}

void BlindMotor::motorPwmSetup() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_12_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 16000,
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
        },
        .deconfigure = false
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.gpio_num = MOTOR_B_PIN;
    ledc_channel.channel = LEDC_CHANNEL_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void BlindMotor::feedbackSetup() {
    // Setup power pin
    gpio_config_t gpioConfig = {
        .pin_bit_mask = BIT64(ENCODER_PWR),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpioConfig);
    gpio_set_level(ENCODER_PWR, 1);

    pcnt_unit_config_t unit_config = {
        .clk_src = PCNT_CLK_SRC_DEFAULT,
        .low_limit = -WATCH_RESOLUTION,
        .high_limit = WATCH_RESOLUTION,
        .intr_priority = 0,
        .flags = {
            .accum_count = 0
        }
    };
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
            .virt_level_io_level = false
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
            .virt_level_io_level = false
        }
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, -WATCH_RESOLUTION));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(pcnt_unit, WATCH_RESOLUTION));

    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_cb,
    };
    motorQueue = xQueueCreate(10, sizeof(int));
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, motorQueue));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

void BlindMotor::updateDesired(const int8_t speed) {
    bool sign = speed < 0;
    int8_t speedAbs = sign ? -speed : speed;

    uint16_t speedRange = _maxSpeed - _minSpeed;
    int32_t lerp = _minSpeed + (((speedAbs - 1) * speedRange) / 99);
    int32_t newSpeed = speed == 0 ? 0 : (sign ? -lerp : lerp);

    updateSpeed(newSpeed);
}

void BlindMotor::updateSpeed(const int32_t speed) {
    if (_speed == 0 && speed != 0 && _prefs != NULL) _prefs->putUShort(NVS_ACTUATIONS, ++_actuations);
    _speed = speed;

    bool forward = speed > 0;
    uint16_t duty = abs(speed) / 8;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, forward ? duty : 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, forward ? 0 : duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

void BlindMotor::init(Preferences* prefs) {
    _prefs = prefs;
    _position = _exactPosition = _target = _prefs->getULong64(NVS_POSITION, INT64_MAX);
    _actuations = _prefs->getUShort(NVS_ACTUATIONS, 0);

    motorPwmSetup();
    feedbackSetup();

    xTaskCreate(motor_task, "Motor", 4096, NULL, 4, NULL);
}

uint64_t absDiff(uint64_t a, uint64_t b) {
    return a > b ? a - b : b - a;
}

void BlindMotor::receiveQueue(bool wait) {
    int eventCount = 0;
    if (xQueueReceive(motorQueue, &eventCount, wait ? portMAX_DELAY : 0)) {
        _position += eventCount;
        // printf("Watch point event, count: %d, result: %llu\n", eventCount, _position);
    }
}

void BlindMotor::task() {
    uint64_t lastLocalPosition = 0;
    uint64_t diff = 0;
    uint8_t movingCheck = 0;
    int8_t localSpeed = 0;
    while (true) {
        // Allow sleep while there are no events
        receiveQueue(true);

        if (_identify) continue;

        movingCheck = 3;
        do {
            int offset = 0;
            pcnt_unit_get_count(pcnt_unit, &offset);

            _exactPosition = _position + offset;
            if (lastLocalPosition != _exactPosition) movingCheck = 3;
            lastLocalPosition = _exactPosition;

            if (_on_move != NULL) {
                uint8_t percent = ((_exactPosition - _min) * 100) / (_max - _min);
                _on_move(percent, getPosition(), _actuations);
            }

            diff = absDiff(_exactPosition, _target);
            if (diff < 100 && _speed == 0) break; // Don't start moving for small distances

            bool dir = _exactPosition < _target;
            int8_t desiredSpeed = (diff < 1000 ? 1 : 100) * (dir ? 1 : -1);
            uint8_t change = ((desiredSpeed ^ localSpeed) < 0) && desiredSpeed != 0 ? 3 : 1;
            if (diff < 20) {
                localSpeed = 0;
            } else if (desiredSpeed > localSpeed) {
                localSpeed += change;
            } else if (desiredSpeed < localSpeed) {
                localSpeed -= change;
            }

            updateDesired(localSpeed);

            vTaskDelay(20 / portTICK_PERIOD_MS);
            receiveQueue(false);
        } while (movingCheck-- > 0);

        if (_prefs != NULL) _prefs->putULong64(NVS_POSITION, _exactPosition);
        // printf("Stopped receiving motor updates. Stopped or stalled? %d, %llu, %llu\n", _speed, _exactPosition, _target);
    }
}

void BlindMotor::updateTarget(const uint64_t newTarget) {
    _target = newTarget;

    if (motorQueue != NULL) {
        int eventCount = 0;
        xQueueGenericSend(motorQueue, &eventCount, 0, queueSEND_TO_BACK);
    }
}

void BlindMotor::goDirection(const bool up) {
    if (_setup) {
        // In setup move forever
        updateTarget(up ? 0 : UINT64_MAX);
    } else {
        goPercent(up ? 0 : 100);
    }
}

void BlindMotor::goPosition(const uint64_t p) {
    if (p < _min || p > _max) return;
    updateTarget(p);
}

void BlindMotor::goPercent(const uint8_t p) {
    if (p > 100) return;
    updateTarget(_min + (uint64_t) ((p / 100.0) * (_max - _min)));
}

void BlindMotor::stop() {
    _target = _exactPosition;
    // No need to trigger task as if it's not running the motor isn't moving
}

uint64_t BlindMotor::setMin() {
    return _min = _exactPosition;
}

uint64_t BlindMotor::setMax() {
    return _max = _exactPosition;
}

void BlindMotor::identify() {
    // Don't run identify while motor is running
    if (_speed != 0 || motorQueue == NULL) return;

    _identify = true;
    updateDesired(100);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    updateDesired(0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    _identify = false;

    int eventCount = 0;
    xQueueGenericSend(motorQueue, &eventCount, 0, queueSEND_TO_BACK);
}

void BlindMotor::setSetup(const bool setup) {
    _setup = setup;
}

uint16_t BlindMotor::getPosition() {
    // Zigbee lower resolution position
    return (_exactPosition - _min) / (1 << 4);
}

void BlindMotor::setVelocity(const uint16_t v) {
    _maxSpeed = (v / 8) + 24000;
}

void BlindMotor::setEnds(const uint64_t min, const uint64_t max) {
    _min = min;
    _max = max;
}

void BlindMotor::nudge(const int16_t dist) {
    uint64_t newTarget = _exactPosition + dist;
    if (!_setup && (newTarget < _min || newTarget > _max)) return;
    updateTarget(newTarget);
}

void BlindMotor::moveCallback(void (*callback)(uint8_t, uint16_t, uint16_t)) {
    _on_move = callback;
}

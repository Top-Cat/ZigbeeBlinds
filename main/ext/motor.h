#include "../config.h"

#include "driver/pulse_cnt.h"
#include "../prefs.h"

#define WATCH_RESOLUTION INT16_MAX
#define NVS_ACTUATIONS        "actuations"
#define NVS_POSITION          "motorPos"

class BlindMotor {
    private:
        void updateSpeed(const int32_t speed);
        void updateDesired(const int8_t speed);
        void motorPwmSetup();
        void pcntSetup();
        void feedbackSetup();

        bool _sensorPower = true;
        void sensorPower(const bool p);

        pcnt_unit_handle_t pcnt_unit = NULL;
        pcnt_channel_handle_t pcnt_chan_a = NULL;
        pcnt_channel_handle_t pcnt_chan_b = NULL;
        uint64_t _position = INT64_MAX;
        uint64_t _exactPosition = INT64_MAX;
        uint64_t _target = INT64_MAX;

        void receiveQueue(const bool wait = false);

        Preferences* _prefs;
        uint64_t _min = 0;
        uint64_t _max = UINT64_MAX;
        int32_t _maxSpeed = 24000;
        int32_t _minSpeed = 22000;
        uint16_t _offset = 0;
        bool _offsetDir = false;
        int32_t _speed = 0;
        uint16_t _actuations = 0;
        bool _setup = false;
        bool _identify = false;

        void updateTarget(const uint64_t newTarget);
        void (*_on_move)(const uint8_t, const uint16_t, const uint16_t);
    public:
        void init(Preferences* prefs);
        void goDirection(const bool up);
        void goPosition(const uint64_t p);
        void goPercent(const uint8_t p);
        void stop();

        void identify();
        void setSetup(const bool setup);
        uint16_t getPosition();
        uint8_t getPercent();
        void setVelocity(const uint16_t v);

        uint64_t setMin();
        uint64_t setMax();
        void setMinSpeed(const int32_t speed);
        void setOffset(const uint16_t offset, const bool dir);
        void setEnds(const uint64_t min, const uint64_t max);
        void nudge(const int16_t dist);
        void moveCallback(void (*callback)(const uint8_t, const uint16_t, const uint16_t));

        void task();
};

extern BlindMotor motor;

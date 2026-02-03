#include "../config.h"

class BlindMotor {
    private:
        void updateSpeed(int16_t speed);
        void motorPwmSetup();
        void feedbackSetup();

        uint64_t _position = UINT32_MAX;
        uint64_t _target = UINT32_MAX;
        uint64_t _min = UINT32_MAX;
        uint64_t _max = UINT32_MAX;
    public:
        void init();
        void goPosition(uint64_t p);
        void goPercent(uint8_t p);
        void stop();

        void setMin();
        void setMax();

        void task();
};

extern BlindMotor motor;

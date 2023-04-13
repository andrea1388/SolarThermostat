#include <stdio.h>
class TTimer
{
    public:
        bool isElapsed();
        uint32_t value; // in ms
        static uint32_t millis();
    private:
        uint32_t tlastFired=0;
};
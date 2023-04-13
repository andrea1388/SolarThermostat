#include <stdio.h>
#include "TTimer.h"
#include "esp_timer.h"
bool TTimer::isElapsed()
{
    uint32_t now=millis();
    if((now-tlastFired)>value)
    {
        tlastFired=now;
        return true;
    } 
    return false;
}

uint32_t TTimer::millis() 
{
    return (esp_timer_get_time() >> 10) && 0xffffffff;
}
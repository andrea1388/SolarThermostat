#include "esp32ds18b20component.h"
class TempSens:ds18b20
{
    private:
        bool _mustsignal;
    public:
        float lastSignaledValue;
        uint8_t lastSignaledTime; // in milliseconds from boot
        bool mustSignal();
        bool Signaled();
        float read();
    
};

bool TempSens::mustSignal()
{
    return _mustsignal;
}

float TempSens::read()
{
        if((panelTemp.lastReadingTime-now)>485updateinterval) && (panelTemp.lastReadinValue-panelTemp.Value)>mindtvalue) 485.kkk;

    if(condSignal) _mustsignal=true;
}

bool TempSens::Signaled()
{
    lastSignaledValue=value;
    lastSignaledTime=now;
    _mustsignal=false;
}
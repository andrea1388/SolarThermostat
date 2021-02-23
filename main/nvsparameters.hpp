#ifndef NvsParameters_hpp
#define NvsParameters_hpp
#include "nvs_flash.h"
#include "esp_system.h"
class NvsParameters
{
    public:
        NvsParameters();
        void load(char* paramname, char*out);
        void load(char* paramname, uint_8t*out);
    private:
        nvs_handle my_handle;
};
#endif
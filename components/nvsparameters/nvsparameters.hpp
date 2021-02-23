#ifndef NvsParameters_hpp
#define NvsParameters_hpp
#include "nvs_flash.h"

class NvsParameters
{
    public:
        NvsParameters();
        void load(const char* paramname, char *out);
        void load(const char* paramname, uint8_t *out);
    private:
        nvs_handle my_handle;
};
#endif
#ifndef NvsParameters_hpp
#define NvsParameters_hpp
#include "nvs_flash.h"
class NvsParameters
{
    public:
        NvsParameters();
        load(char* paramname, char*out);
        load(char* paramname, uint_8t*out);
    private:
        nvs_handle my_handle;
};
#endif
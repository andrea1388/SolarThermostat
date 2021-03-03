#ifndef NvsParameters_hpp
#define NvsParameters_hpp
#include "nvs_flash.h"

class NvsParameters
{
    public:
        void Init();
        void load(const char* paramname, char **out);
        void load(const char* paramname, uint8_t *out);
        esp_err_t save(const char* paramname, uint8_t val);
        esp_err_t save(const char* paramname, char *val);
    private:
        nvs_handle my_handle;
};
#endif
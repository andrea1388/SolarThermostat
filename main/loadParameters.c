#include "esp_system.h"
#include "esp_flash.h"
#include "nvs_flash.h"

extern uint32_t Tread; 
extern uint32_t Tsendtemps; 
extern uint32_t Ton;
extern uint32_t Toff;
extern float deltaT;

void loadParameters() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    nvs_handle my_handle;
    esp_err_t r= nvs_open("storage", NVS_READWRITE, &my_handle);
    switch(r)
    {
        case ESP_OK:
            nvs_get_u32(my_handle, "Tread", &Tread);
            /* nvs_get_u8(my_handle, "camera", &camera);
            nvs_get_u8(my_handle, "struttura", &struttura);
            nvs_get_u8(my_handle, "modalita", &modalita);
            nvs_get_u8(my_handle, "secondiAperturaSerratura", &secondiAperturaSerratura);
            nvs_get_u8(my_handle, "secondiChiusuraSerratura", &secondiChiusuraSerratura); */
            break;
        
    }


}

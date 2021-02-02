#ifndef WiFi_hpp
#define WiFi_hpp
#include "esp_event.h"
#include <string>
#include <vector>
class WiFi {
    public:
        void Connect();
        void AddNetwork(const char* ssid,const char* password);
        void *callback(esp_event_base_t event_base,int32_t event_id, void* event_data);
    private:
        std::vector<string> ssid;
        std::vector<string> password;

        void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

};
#endif 

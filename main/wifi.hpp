#ifndef WiFi_hpp
#define WiFi_hpp
#include "esp_event.h"
#include <string>
#include <vector>
using namespace std;
namespace WiFi {
        void Connect();
        void AddNetwork(const char* ssid,const char* password);
        inline void (*callback)(esp_event_base_t event_base,int32_t event_id, void* event_data);
}
#endif 

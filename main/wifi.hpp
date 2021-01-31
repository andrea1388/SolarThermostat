#ifndef WiFi_hpp
#define WiFi_hpp

#include <string>
#include <vector>
class WiFi {
    public:
        void Connect();
        void AddNetwork(char* ssid,char* password);
    private:
        vector<string> ssid;
        vector<string> password;

        void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)

}
#endif 

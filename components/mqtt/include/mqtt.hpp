#ifndef MQTT_hpp
#define MQTT_hpp

class Mqtt {
    public:
        void Connect(const char* username, const char* password,const char* uri,const char* cert);
        void Start();
        void Stop();
        void (*onEvent)(WiFi* wifi, uint8_t ev);
        vector<string> ssid;
        vector<string> password;

    private:
        esp_mqtt_client_handle_t client;
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
        
    
        
};
#endif 

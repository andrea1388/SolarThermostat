#ifndef MQTT_hpp
#define MQTT_hpp
#include "mqtt_client.h"

class Mqtt {
    public:
        void Init(const char* username, const char* password,const char* uri,const char* cert);
        void Start();
        void Stop();
        void (*onEvent)(Mqtt* mqtt,esp_mqtt_event_handle_t event);


    private:
        esp_mqtt_client_handle_t client;
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
        
    
        
};
#endif 

#include "mqtt.h"
using namespace std;


void Mqtt::Start()
{
    esp_mqtt_client_start(client);
}

void Mqtt::Stop()
{
    esp_mqtt_client_stop(client);
}

void Mqtt::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    Mqtt *obj=(Mqtt *)handler_args;
    if(obj->onEvent!=NULL) (*obj->onEvent)(obj,(esp_mqtt_event_handle_t)event_data);
    /*
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event=event_data;
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    
    */
    return ESP_OK;

}


void Mqtt::Init(const char* _username, const char* _password,const char* _uri,const char* _cert)
{
    //esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.uri=_uri;
    mqtt_cfg.username=_username;
    mqtt_cfg.password=_password;
    mqtt_cfg.cert_pem=_cert;
    mqtt_cfg.skip_cert_common_name_check=true;
    mqtt_cfg.disable_auto_reconnect=false;
    client = esp_mqtt_client_init(&mqtt_cfg);
    if(client==NULL) {
        //ESP_LOGE(TAG,"esp_mqtt_client_init fails");
    } else {
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, &Mqtt::mqtt_event_handler, (void*)this);
    }
}





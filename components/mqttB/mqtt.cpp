#include "mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
static const char* TAG = "Mqtt";

void Mqtt::Start()
{
    esp_mqtt_client_start(client);
}

void Mqtt::Stop()
{
    esp_mqtt_client_stop(client);
}

int Mqtt::Publish(char *topic,char *msg) {
    //if((xEventGroupGetBits(s_wifi_event_group) & MQTT_CONNECTED_BIT) == false) return;
    ESP_LOGI(TAG, "Mqtt::Publish topic=%s, msg=%s", topic, msg);
    return esp_mqtt_client_publish(client, topic, msg, strlen(msg), 0, 0);

}
int Mqtt::Subscribe(char *topic)
{
    ESP_LOGI(TAG, "Mqtt::Subscribe topic=%s", topic);
    return esp_mqtt_client_subscribe(client, topic, 0);
}

void Mqtt::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    Mqtt *obj=(Mqtt *)handler_args;
    if(obj->onEvent!=NULL) (*obj->onEvent)(obj,(esp_mqtt_event_handle_t)event_data);
    /*
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event=event_data;
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    
    */

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
        esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, &Mqtt::mqtt_event_handler, (void*)this);
    }
}





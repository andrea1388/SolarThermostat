#include "wifi.hpp"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"
#include <string>
#include <vector>
using namespace std;


void Mqtt::Start();
{
    esp_mqtt_client_start(client);
}

void Mqtt::Stop();
{
    esp_mqtt_client_stop(client);
}

static void Mqtt::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event=event_data;
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, "ciao", 0);
            xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            Publish("bueo","SolarThermostat started");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if (strncmp(event->data, "send binary please", event->data_len) == 0) {
                ESP_LOGI(TAG, "Sending the binary");
                //send_binary(client);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;

}

void Mqtt::Connect()
{
    esp_wifi_connect();
}
void Mqtt::Init(const char* _username, const char* _password,const char* _uri,const char* _cert)
{
    //esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    const esp_mqtt_client_config_t mqtt_cfg = {};
    strcpy(mqtt_cfg.uri,uri);
    strcpy(mqtt_cfg.username,username);
    strcpy(mqtt_cfg.password,password);

        .uri = CONFIG_BROKER_URI,
        .cert_pem = (const char *)ca_crt_start,
        .username = "homeassistant",
        .password = "iw3gcb",
        //.client_key_pem = (const char *)client_key_start,
        //.client_cert_pem = (const char *)client_crt_start,
        .skip_cert_common_name_check=true
        //.disable_auto_reconnect=true
        
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    if(client==NULL) {
        ESP_LOGE(TAG,"esp_mqtt_client_init fails");
    } else {
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    }
}





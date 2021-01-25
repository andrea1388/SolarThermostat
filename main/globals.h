#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
esp_mqtt_client_handle_t client;
extern EventGroupHandle_t s_wifi_event_group;

#define GPIO_SENS_PANEL GPIO_NUM_1
#define GPIO_SENS_TANK GPIO_NUM_2
#define GPIO_PUMP GPIO_NUM_3
#define GPIO_LED GPIO_NUM_4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT2
#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

extern EventGroupHandle_t s_wifi_event_group;

#define GPIO_SENS_PANEL GPIO_NUM_21
#define GPIO_SENS_TANK GPIO_NUM_23
#define GPIO_PUMP GPIO_NUM_18
#define GPIO_LED GPIO_NUM_4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT2
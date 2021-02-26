/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
// https://github.com/UncleRus/esp-idf-lib
// https://esp-idf-lib.readthedocs.io/en/latest/groups/ds18x20.html
#include <ds18x20.h>
#include "globals.h"
//#include <iostream>


#include "wifi.hpp"
#include "mqtt.h"
#include "nvsparameters.hpp"
#define MAXCMDLEN 200
#define TOTACheck 24

static const char *TAG = "main";

extern "C" {
    void app_main(void);
    extern void simple_ota_example_task(void *pvParameter);
}

// global objects
//EventGroupHandle_t s_wifi_event_group;
float Tp,Tt; // store the last temp read
int64_t now; // milliseconds from startup
uint8_t Tread=1; // interval in seconds between temperature readings
uint8_t Tsendtemps=1; // interval in minutes between temperature transmissions to mqtt broker
uint8_t Ton=40; // On time of the pump (usually is the time required to empty the panel) 
uint8_t Toff=40; // Off time
uint8_t DT_TxMqtt=2; // if one of the two value of temperature red exceeds this delta, then values are transmitted to mqtt
uint8_t DT_ActPump=2; // if Tpanel > Ttank + DT_ActPump, then pump is acted
char MqttTpTopic[]="SolarThermostat/Tp";
char MqttTtTopic[]="SolarThermostat/Tt";
char MqttControlTopic[]="SolarThermostat/control";
bool otacheck=true;
Mqtt mqtt;
WiFi wifi;
NvsParameters param;
ds18x20_addr_t panel_sens[1];
ds18x20_addr_t tank_sens[1];
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");

void onNewCommand(char *s);

void MqttEvent(Mqtt* mqtt, esp_mqtt_event_handle_t event)
{
    switch (event->event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt->Subscribe(MqttControlTopic);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_DATA:
            if(strcmp(event->topic,MqttControlTopic)==0) onNewCommand(event->data);
            /*
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            */
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
}
void WiFiEvent(WiFi* wifi, uint8_t ev)
{
    switch(ev)
    {
        case WIFI_START: // start
            wifi->Connect();
            break;
        case WIFI_DISCONNECT: // disconnected
            ESP_LOGI(TAG,"Disconnected.");
            wifi->Connect();
            gpio_set_level(GPIO_LED,0);
            mqtt.Stop();
            break;
        case WIFI_GOT_IP: // connected
            ESP_LOGI(TAG,"GotIP");
            //ESP_LOGI("Connected. ip=%s",wifi->ip);
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            gpio_set_level(GPIO_LED,1);
            mqtt.Start();
            xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
            break;

    }
}

void onNewCommand(char *s)
{
    uint8_t err=0;
    const char *delim=" ";
    char *token = strtok(s, delim);
    
    // mqtt uri
    if (strcmp(token,"mqtturi")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_uri",token);
        }
    }

    // mqtt username
    if (strcmp(token,"mqttuname")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_username",token);
        }
    }

    // mqtt password
    if (strcmp(token,"mqttpwd")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_password",token);
        }
    }

    // restart
    if (strcmp(token,"restart")==0)
    {
        esp_restart();
    }

    if (strcmp(token,"dtpump")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0)
            {
                DT_ActPump=j;
                param.save("DT_ActPump",DT_ActPump);
            } 
        }
    }

    if (strcmp(token,"dtmqtt")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0)
            {
                DT_TxMqtt=j;
                param.save("DT_TxMqtt",DT_TxMqtt);
            } 
        }
    }

    if (strcmp(token,"tsendtemps")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0)
            {
                Tsendtemps=j;
                param.save("Tsendtemps",Tsendtemps);
            } 
        }
    }

    if (strcmp(token,"tread")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0) {
                Tread=j;
                param.save("Tread",Tread);
            }
        }
    }

    if (strcmp(token,"ton")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0) {
                Ton=j;
                param.save("Ton",Ton);
            }
        }
    }

    if (strcmp(token,"toff")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0) {
                Toff=j;
                param.save("Toff",Toff);
            }
        }
    }

    if(err==1)
    {
        printf("missing parameter\n");
    }
    if(err==2)
    {
        printf("wrong value\n");
    }
}

void ProcessThermostat() {
    static int64_t tchange=0;
    static bool needPumpprec=false;
    static uint8_t state=0;
    bool needPump=(Tp > Tt + DT_ActPump); // condition that pump action is needed
    bool condA = ((needPump==true) && (needPumpprec==false) && (state==0)); 
    bool condB = ((state==0) && (needPump==true) && ((now - tchange) > Toff));
    bool condC = ((state==1) && ((now - tchange) > Ton));
    ESP_LOGI(TAG, "cond np,a,b,c: %u,%u,%u,%u - state:%u",needPump,condA,condB,condC,state);
    if( condA || condB ) {
        state=1;
        gpio_set_level(GPIO_PUMP,1);
        tchange=now;
    }
    if( condC ) {
        state=0;
        gpio_set_level(GPIO_PUMP,0);
        tchange=now;
    }
    needPumpprec=needPump;

}

void ReadTemperatures() {
    ds18x20_measure(GPIO_SENS_PANEL, panel_sens[0], true);
    ds18x20_read_temperature(GPIO_SENS_PANEL, panel_sens[0], &Tp);
    vTaskDelay(1);
    ds18x20_measure(GPIO_SENS_TANK, tank_sens[0], true);
    ds18x20_read_temperature(GPIO_SENS_TANK, tank_sens[0], &Tt);
    vTaskDelay(1);
    ESP_LOGI(TAG, "Tp, Tt: %.1f,%.1f",Tp,Tt);
}

void ProcessStdin() {
    static char cmd[MAXCMDLEN];
    static uint8_t cmdlen=0;
    int c = fgetc(stdin);
    if(c!= EOF) 
    {
        if(c=='\n') {
            cmd[cmdlen]=0;
            onNewCommand(cmd);
            cmdlen=0;
        }
        cmd[cmdlen++]=tolower(c);
        if(cmdlen==MAXCMDLEN-1) {
            cmdlen=0;
        }
    }
}


void app_main(void)
{

    int64_t tlastread=0;  // time of the last temperatures read
    int64_t tlastsenttemp=0;  // time of the last temperatures send
    int64_t tlastotacheck=0;
    float Tplast=0; // previous reading of Tp
    float Ttlast=0; // previous reading of Tt

    //srand((unsigned int)esp_timer_get_time());
    //s_wifi_event_group = xEventGroupCreate();

    // setup gpio
    gpio_set_direction(GPIO_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED,0);
    gpio_set_level(GPIO_PUMP,0);
    esp_log_level_set("*", ESP_LOG_INFO);

    // configure wifi
    wifi.onEvent=&WiFiEvent;
    wifi.Start("Mordor","gandalfilgrigio");

    //configure mqtt
    char *username=NULL;
    char *password=NULL;
    char *uri=NULL;
    param.load("mqtt_username",username);
    param.load("mqtt_password",password);
    param.load("mqtt_uri",uri);
    mqtt.Init(username,password,uri,(const char*)ca_crt_start);
    mqtt.onEvent=&MqttEvent;
    free(username);
    free(password);
    free(uri);


    param.load("Tread",&Tread);
    param.load("Tsendtemps",&Tsendtemps);
    param.load("Ton",&Ton);
    param.load("Toff",&Toff);
    param.load("DT_TxMqtt",&DT_TxMqtt);
    param.load("DT_ActPump",&DT_ActPump);

    
    // configure ds18b20s
    int sensor_count = ds18x20_scan_devices(GPIO_SENS_PANEL, panel_sens, 1);
    if (sensor_count < 1) ESP_LOGE(TAG,"Panel sensor not detected");
    sensor_count = ds18x20_scan_devices(GPIO_SENS_TANK, tank_sens, 1);
    if (sensor_count < 1) ESP_LOGE(TAG,"Tank sensor not detected");
    
    

    /*
        Main cycle:
        periodically (Tpoll) read temperatures Tp and Tt and act the pump if necessary
        periodically (Tsendtemps) send Tp and Tt to MQTT broker, but only if they differ more than a certain amount (deltaT)
        read characters from stdin and respond to commands issued
    */

    

    while(true) {
        now=(esp_timer_get_time()/1000);
        if(((now - tlastotacheck)/1440000) > TOTACheck) otacheck=true; // force a ota check every TOTACheck hours
        ProcessStdin();

        if(otacheck){
            simple_ota_example_task(NULL);
            otacheck=false;
        }       
        
        
        if(((now - tlastread)/1000) > Tread) {
            tlastread=now;
            char msg[10];
            ReadTemperatures();
            ProcessThermostat();
            if(((now - tlastsenttemp)/60000) > Tsendtemps) {
                if( abs(Tp-Tplast) > DT_TxMqtt || abs(Tt-Ttlast) > DT_TxMqtt ) {
                    sprintf(msg,"%.1f",Tp);
                    mqtt.Publish(MqttTpTopic,msg);
                    sprintf(msg,"%.1f",Tt);
                    mqtt.Publish(MqttTtTopic,msg);
                    Tplast=Tp;
                    Ttlast=Tt;
                    tlastsenttemp=now;
                }
            }
        }

            
        vTaskDelay(1);

    }
}




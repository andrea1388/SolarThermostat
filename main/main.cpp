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
#include <iostream>
#include "cmdDecoder.hpp"
#include "wifi.hpp"
#define MAXCMDLEN 100

static const char *TAG = "main";

extern "C" {
    void app_main(void);
    extern void loadParameters();
    extern void wifi_init_sta();
    extern void simple_ota_example_task(void *pvParameter);
    extern void mqtt_app_start(void);
    extern void Publish(char *data,char*);
}


EventGroupHandle_t s_wifi_event_group;
ds18x20_addr_t panel_sens[1];
ds18x20_addr_t tank_sens[1];
float Tp,Tt; // store the last temp read
int64_t now; // milliseconds from startup
uint32_t Tread; // interval in milliseconds between temperature readings
uint32_t Tsendtemps; // interval in milliseconds between temperature transmissions to mqtt broker
uint32_t Ton; // On time of the pump (usually is the time required to empty the panel) 
uint32_t Toff; // Off time
float deltaT; // delta T %, if one of the tho tenps red exceeds this delta then temp are transmitted to mqtt
float TempMargin=2.0;
char MqttTpTopic[]="SolarThermostat/Tp";
char MqttTtTopic[]="SolarThermostat/Tt";
char MqttControlTopic[]="SolarThermostat/control";
char MqttStatusTopic[]="SolarThermostat/status";
bool otacheck=true;
cmdDecoder decoder;

void ProcessThermostat() {
    static int64_t tchange=0;
    static bool needPumpprec=false;
    static uint8_t state=0;
    bool needPump=(Tp > Tt + TempMargin); // condition that pump action is needed
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
            if(decoder.parse(cmd)) {

            } else cout << decoder.err << endl;
            
            cmdlen=0;
        }
        cmd[cmdlen++]=tolower(c);
        if(cmdlen==MAXCMDLEN-1) {
            cmdlen=0;
        }
    }
}

void wifievent(esp_event_base_t event_base,int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        gpio_set_level(GPIO_LED,1);
        esp_mqtt_client_start(client);
        xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        gpio_set_level(GPIO_LED,0);
        esp_mqtt_client_stop(client);
    }

}

using namespace std;


void app_main(void)
{
    Tread = 1000;
    Tsendtemps= 30*1000; // 10 minutes
    Ton= 40*1000;
    Toff = 40*1000;
    deltaT = 2.0; 
    loadParameters();
    srand((unsigned int)esp_timer_get_time());
    s_wifi_event_group = xEventGroupCreate();
    //WiFi wifi;
    WiFi::AddNetwork("Mordor","gandalfilgrigio");
    WiFi::callback=&wifievent;
    WiFi::Connect();
    //wifi_init_sta();
    //xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
    gpio_set_direction(GPIO_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED,0);
    gpio_set_level(GPIO_PUMP,0);
    esp_log_level_set("*", ESP_LOG_INFO);
/*     esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE); */
    mqtt_app_start();
    

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
    int64_t tlastread=0;  // time of the last temperatures read
    int64_t tlastsenttemp=0;  // time of the last temperatures send
    int64_t tlastotacheck=0;
    float Tplast=0; // previous reading of Tp
    float Ttlast=0; // previous reading of Tt
    
    
    #define TOTACheck 10000000
    
    

    while(true) {
        now=(esp_timer_get_time()/1000);
        if((now - tlastotacheck) > TOTACheck) otacheck=true; // force a ota check every TOTACheck milliseconds
        ProcessStdin();

        if(otacheck){
            simple_ota_example_task(NULL);
            otacheck=false;
        }       
        
        
        if((now - tlastread) > Tread) {
            tlastread=now;
            char msg[10];
            ReadTemperatures();
            ProcessThermostat();
            if((now - tlastsenttemp)> Tsendtemps) {
                if( abs(Tp-Tplast) > deltaT || abs(Tt-Ttlast) > deltaT ) {
                    sprintf(msg,"%.1f",Tp);
                    Publish(MqttTpTopic,msg);
                    sprintf(msg,"%.1f",Tt);
                    Publish(MqttTtTopic,msg);
                    Tplast=Tp;
                    Ttlast=Tt;
                    tlastsenttemp=now;
                }
            }
        }

            
        vTaskDelay(1);

    }
}




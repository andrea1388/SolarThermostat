/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "esp_log.h"
// https://github.com/UncleRus/esp-idf-lib
// https://esp-idf-lib.readthedocs.io/en/latest/groups/ds18x20.html
#include <ds18x20.h>


#define GPIO_SENS_PANEL 1
#define GPIO_SENS_TANK 2
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT2

extern void initGPIO();
extern void loadParameters();
extern void wifi_init_sta();
extern void simple_ota_example_task(void *pvParameter);
extern void mqtt_app_start(void);
extern void pumpOnOff(bool on);
extern void Publish(char *data,char*);
extern void ReadTemperatures();

EventGroupHandle_t s_wifi_event_group;
float Tp,Tt; // store the last temp read
int64_t now; // milliseconds from startup
uint32_t Tread; // interval in milliseconds between temperature readings
uint32_t Tsendtemps; // interval in milliseconds between temperature transmissions to mqtt broker
uint32_t Ton; // On time of the pump (usually is the time required to empty the panel) 
uint32_t Toff; // Off time
float deltaT; // delta T %, if one of the tho tenps red exceeds this delta then temp are transmitted to mqtt
float TempMargin=2.0;
char MqttTpTopic[]="SolarThermostat/Tp";
char MqttTtTopic[]="SolarThermostat/Tp";
char MqttControlTopic[]="SolarThermostat/control";
char MqttStatusTopic[]="SolarThermostat/status";

void ProcessThermostat() {
    static int64_t tchange=0;
    static bool needPumpprec=false;
    static uint8_t state=0;
    bool needPump=(Tp > Tp + TempMargin);
    bool condA = ((needPump==true) && (needPumpprec==false) && (state==0));
    bool condB = ((state==0) && (needPump==true) && ((now - tchange) > Toff));
    bool condC = ((state==1) && ((now - tchange) > Ton));
    
    if( condA || condB ) {
        state=1;
        pumpOnOff(true);
        tchange=now;
    }
    if( condC ) {
        state=0;
        pumpOnOff(false);
        tchange=now;
    }
    needPumpprec=needPump;

}

void ReadTemperatures() {
    ds18x20_measure(GPIO_SENS_PANEL, ds18x20_ANY, false);
    ds18x20_measure(GPIO_SENS_TANK, ds18x20_ANY, false);
    vTaskDelay(1);
    ds18x20_read_temperature(GPIO_SENS_PANEL, ds18x20_ANY, &Tp);
    ds18x20_read_temperature(GPIO_SENS_TANK, ds18x20_ANY, &Tt);

}

void app_main(void)
{
    Tread = 1000;
    Tsendtemps= 10*60*1000; // 10 minutes
    Ton= 40*1000;
    Toff = 4*60*1000;
    deltaT = 0.1; // 10%
    loadParameters();
    initGPIO();
    s_wifi_event_group = xEventGroupCreate();
    wifi_init_sta();
    //xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
    
    esp_log_level_set("*", ESP_LOG_INFO);
/*     esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE); */
    mqtt_app_start();
    
/*     ds18x20_addr_t panel_sens[1];
    ds18x20_addr_t tank_sens[1];
    sensor_count = ds18x20_scan_devices(GPIO_SENS_PANEL, panel_sens, 1);
    if (sensor_count < 1) ESP_LOGE(TAG,"Panel sensor not detected");
    sensor_count = ds18x20_scan_devices(GPIO_SENS_TANK, tank_sens, 1);
    if (sensor_count < 1) ESP_LOGE(TAG,"Tank sensor not detected"); */
    
    

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
    uint8_t cmdlen=0;
    #define MAXCMDLEN 100
    #define TOTACheck 10000000
    char cmd[MAXCMDLEN];
    bool otacheck=true;

    while(true) {
        now=(esp_timer_get_time()/1000);
        if((now - tlastotacheck) > TOTACheck) otacheck=true;
        int c = fgetc(stdin);
        if(c!= EOF) 
        {
            if(c=='\n') {
                cmd[cmdlen]=0;
                printf("cmd: %s\n",cmd);
                if(strcmp(cmd,"o")==1) {
                    otacheck=true;
                    printf("ota check\n");
                }
                cmdlen=0;
            }
            cmd[cmdlen++]=c;
            if(cmdlen==MAXCMDLEN-1) {
                cmdlen=0;
            }
        }
        if(otacheck){
            if(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) simple_ota_example_task(NULL);
            otacheck=false;
        }       
        
        
        if((now - tlastread) > Tread) {
            tlastread=now;
            ReadTemperatures();
            ProcessThermostat();
            if((now - tlastsenttemp)> Tsendtemps) {
                tlastsenttemp=now;
                if( abs(Tp-Tplast)/Tplast > deltaT || abs(Tt-Ttlast)/Tplast > deltaT ) {
                    char msg[10];
                    sprintf(msg,"%.1f",Tp);
                    Publish(MqttTpTopic,msg);
                    sprintf(msg,"%.1f",Tt);
                    Publish(MqttTtTopic,msg);
                    Tplast=Tp;
                    Ttlast=Tt;
                }
            }
        }

            
        vTaskDelay(1);

    }
}




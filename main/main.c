/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"



#define GPIO_SENS_PANEL 1
#define GPIO_SENS_TANK 2


extern void initGPIO();
extern void loadParameters();
extern void wifi_init_sta();
extern void simple_ota_example_task(void *pvParameter);
extern void mqtt_app_start(void);
extern void pumpOnOff(bool on);

EventGroupHandle_t s_wifi_event_group;
float Tp,Tt; // store the last temp read
int64_t now; // milliseconds from startup
int32_t Tread; // interval in milliseconds between temperature readings
int32_t Tsendtemps; // interval in milliseconds between temperature transmissions to mqtt broker
int32_t Ton; // On time of the pump (usually is the time required to empty the panel) 
int32_t Toff; // Off time
float deltaT; // delta T %, if one of the tho tenps red exceeds this delta then temp are transmitted to mqtt


void app_main(void)
{
    Tread = 1000;
    Tsendtemps= 10*60*1000; // 10 minutes
    Ton= 40*1000;
    Toff = 4*60*1000;
    deltaT = 0.1; // 10%

    loadParameters();
    initGPIO();
    wifi_init_sta();
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
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
    int64_t tlastread;  // time of the last temperatures read
    int64_t tlasttempsend;  // time of the last temperatures send
    float Tplast; // previous reading of Tp
    float Ttlast; // previous reading of Tt
    uint8_t cmdlen=0;
    #define MAXCMDLEN 100
    char[MAXCMDLEN] command;

    while(true) {
        vTaskDelay(1);
        //fgets(url_buf, OTA_URL_SIZE, stdin);
        int c = fgetc(stdin);
        if(c!= EOF) 
        {
            if(c='\n') {
                command[cmdlen]=0;
                printf("cmd: %s",command);s
            }
            command[cmdlen++]=c;
            if(cmdlen==MAXCMDLEN-1) {
                cmdlen=0;
            }
        }
        now=(esp_timer_get_time()/1000);
        
        if((now - tlastread) > Tread) {
            tlastread=now;
            ReadTemperatures();
            ProcessThermostat();
            if((now - tlastsenttemp)> Tsendtemps) {
                tlasttempsend=now;
                if( abs(Tp-Tplast)/Tplast > deltaT || abs(Tt-Ttlast)/Tplast > deltaT ) {
                    SendTemps();
                    Tplast=Tp;
                    Ttlast=Tt;
                }
            }
        }

            
        
    }
}

inline void ProcessThermostat() {
    static int64_t tchange=0;
    bool needPump=(Tp > Tp + delta1);
    bool condA = needPump==true && neddPumpprec==false && state==0;
    bool condB = state==0 && needPump==true && ((now - tchange) > Toff));
    bool condC = state==1 && ((now - tchange) > Ton);
    
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
    neddPumpprec=needPump;

}


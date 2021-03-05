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
#include "otafw.h"
//#include <iostream>


#include "wifi.hpp"
#include "mqtt.h"
#include "nvsparameters.hpp"
#define MAXCMDLEN 200
#define TOTACheck 24
#define WIFI_CONNECTED_BIT BIT0
#define OTA_BIT      BIT1
#define GPIO_SENS_PANEL GPIO_NUM_21
#define GPIO_SENS_TANK GPIO_NUM_23
#define GPIO_PUMP GPIO_NUM_18
#define GPIO_LED GPIO_NUM_4
#define VERSION 5

static const char *TAG = "main";

extern "C" {
    void app_main(void);
    extern void simple_ota_example_task(void *pvParameter);
}

// global objects
EventGroupHandle_t event_group;
float Tp,Tt; // store the last temp read
int64_t now; // milliseconds from startup
uint8_t Tread=1; // interval in seconds between temperature readings
uint8_t Tsendtemps=1; // interval in minutes between temperature transmissions to mqtt broker
uint8_t Ton=40; // On time in seconds of the pump (usually is the time required to empty the panel) 
uint8_t Toff=40; // Off time in seconds
uint8_t DT_TxMqtt=2; // if one of the two value of temperature red exceeds this delta, then values are transmitted to mqtt
uint8_t DT_ActPump=2; // if Tpanel > Ttank + DT_ActPump, then pump is acted
char *MqttTpTopic; //  mqtt_tptopic SolarThermostat/Tp
char *MqttTtTopic; //  mqtt_tttopic SolarThermostat/Tt
char *MqttControlTopic; // mqtt_cttopic SolarThermostat/control
char *otaurl; // otaurl https://192.168.1.105:8070/SolarThermostat.bin


Mqtt mqtt;
WiFi wifi;
NvsParameters param;
Otafw otafw;
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
            char msg[6];
            sprintf(msg,"v=%u",VERSION);
            mqtt->Publish(MqttControlTopic,msg);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            if(strncmp(event->topic,MqttControlTopic,event->topic_len)==0) {
                char* buf;
                buf=(char *)malloc(event->data_len+1);
                strncpy(buf,event->data,event->data_len);
                buf[event->data_len]=0;
                onNewCommand(buf);
                free(buf);
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
}
void WiFiEvent(WiFi* wifi, uint8_t ev)
{
    switch(ev)
    {
        case WIFI_START: // start
            wifi->Connect();
            break;
        case WIFI_DISCONNECT: // disconnected
            xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
            wifi->Connect();
            gpio_set_level(GPIO_LED,0);
            mqtt.Stop();
            break;
        case WIFI_GOT_IP: // connected
            ESP_LOGI(TAG,"GotIP");
            //ESP_LOGI("Connected. ip=%s",wifi->ip);
            xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
            gpio_set_level(GPIO_LED,1);
            mqtt.Start();
            break;

    }
}

void onNewCommand(char *s)
{
    uint8_t err=0;
    const char *delim=" ";
    ESP_LOGI(TAG,"New command=%s",s);
    char *token = strtok(s, delim);
    if(!token) return;
    // mqtt uri
    if (strcmp(token,"mqtturi")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_uri",token);
            return;
        }
    }

    // mqtt username
    if (strcmp(token,"mqttuname")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_username",token);
            return;
        }
    }

    // mqtt password
    if (strcmp(token,"mqttpwd")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_password",token);
            return;
        }
    }
    // mqtt_tptopic
    if (strcmp(token,"mqtt_tptopic")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_tptopic",token);
            return;
        }
    }
    if (strcmp(token,"mqtt_tttopic")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_tttopic",token);
            return;
        }
    }
    if (strcmp(token,"mqtt_cttopic")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("mqtt_cttopic",token);
            return;
        }
    }

    // ota url
    if (strcmp(token,"otaurl")==0 )
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("otaurl",token);
            esp_restart();
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
                return;
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
                return;
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
                return;
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
                return;
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
                return;
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
                return;
            }
        }
    }
    if (strcmp(token,"otacheck")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) {
            xEventGroupSetBits(event_group,OTA_BIT);
            return;
        }
    }

    if(err==1)
    {
        printf("missing parameter\n");
        return;
    }
    if(err==2)
    {
        printf("wrong value\n");
        return;
    }
    printf("wrong command\n");
}

void ProcessThermostat() {
    static int64_t tchange=0;
    static bool needPumpprec=false;
    static uint8_t state=0;
    bool needPump=(Tp > Tt + DT_ActPump); // condition that pump action is needed
    bool condA = ((needPump==true) && (needPumpprec==false) && (state==0)); 
    bool condB = ((state==0) && (needPump==true) && (((now - tchange)/1000) >= Toff));
    bool condC = ((state==1) && (((now - tchange)/1000) >= Ton));
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
        if(c=='\n') 
        {
            cmd[cmdlen]=0;
            onNewCommand(cmd);
            cmdlen=0;
        }
        else
        {
            cmd[cmdlen++]=c;
            if(cmdlen==MAXCMDLEN-1) 
            {
                cmdlen=0;
            }
        }
    }
}

void Ota(void *o) 
{
    ESP_LOGI(TAG, "Starting OTA task");
    for (;;) 
    {
        if(xEventGroupWaitBits(event_group,WIFI_CONNECTED_BIT | OTA_BIT, pdFALSE, pdTRUE, 1000/portTICK_PERIOD_MS) == (WIFI_CONNECTED_BIT | OTA_BIT)) 
        {
            ESP_LOGI(TAG, "Starting OTA update");
            otafw.Check();        
            xEventGroupClearBits(event_group, OTA_BIT);
        }
    }
}


void app_main(void)
{
    param.Init();
    int64_t tlastread=0;  // time of the last temperatures read
    int64_t tlastsenttemp=0;  // time of the last temperatures send
    int64_t tlastotacheck=0;
    float Tplast=0; // previous reading of Tp
    float Ttlast=0; // previous reading of Tt

    //srand((unsigned int)esp_timer_get_time());
    event_group = xEventGroupCreate();

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
    param.load("mqtt_username",&username);
    param.load("mqtt_password",&password);
    param.load("mqtt_uri",&uri);
    param.load("mqtt_tptopic",&MqttTpTopic);
    param.load("mqtt_tttopic",&MqttTtTopic);
    param.load("mqtt_cttopic",&MqttControlTopic);
    mqtt.Init(username,password,uri,(const char*)ca_crt_start);
    mqtt.onEvent=&MqttEvent;
    free(username);
    free(password);
    free(uri);

    param.load("otaurl",&otaurl);
    if(otaurl) 
    {
        otafw.Init(otaurl,(const char*)ca_crt_start);
        xTaskCreate(&Ota, "ota_task", 8192, NULL, 5, NULL);
    }

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
    ESP_LOGI(TAG,"Starting. Version=%u Tread=%u Tsendtemps=%u Ton=%u Toff=%u DT_TxMqtt=%u DT_ActPump=%u",VERSION,Tread,Tsendtemps,Ton,Toff,DT_TxMqtt,DT_ActPump);
    /*
        Main cycle:
        periodically (Tpoll) read temperatures Tp and Tt and act the pump if necessary
        periodically (Tsendtemps) send Tp and Tt to MQTT broker, but only if they differ more than a certain amount (deltaT)
        read characters from stdin and respond to commands issued
    */
    while(true) {
        // get timer value in milliseconds from boot
        now=(esp_timer_get_time()/1000);
        // reschedule a otafw check after TOTACheck hours
        if(((now - tlastotacheck)/3600000) >= TOTACheck) {
            xEventGroupSetBits(event_group,OTA_BIT); // force a ota check every TOTACheck hours
            tlastotacheck=now;
        }
        // process commands from stdin
        ProcessStdin();   
        // read temperatures and process 
        if(((now - tlastread)/1000) >= Tread) {
            tlastread=now;
            char msg[10];
            ReadTemperatures();
            ProcessThermostat();
            // send to mqtt broker if it's the case
            if(((now - tlastsenttemp)/60000) >= Tsendtemps) {
                if( abs(Tp-Tplast) >= DT_TxMqtt || abs(Tt-Ttlast) >= DT_TxMqtt ) {
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




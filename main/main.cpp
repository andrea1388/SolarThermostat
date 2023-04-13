#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "freertos/event_groups.h"
#include "esp_timer.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "driver/gpio.h"

#include "otafw.h"



#include "wifi.h"
#include "nvsparameters.h"
#include "BinarySensor.h"
#include "Sensor.h"
#include "Switch.h"
#include "TempSens.h"
#include "TTimer.h"

#define MAXCMDLEN 200
#define TOTACheck 24
#define WIFI_CONNECTED_BIT BIT0
#define OTA_BIT      BIT1
#define GPIO_SENS_PANEL GPIO_NUM_21
#define GPIO_SENS_TANK GPIO_NUM_23
#define GPIO_PUMP GPIO_NUM_18
#define GPIO_LED GPIO_NUM_4
#define GPIO_BUTTON GPIO_NUM_13
#define VERSION 9

static const char *TAG = "main";

void searchTempSensor();
extern "C" {
    void app_main(void);
    extern void simple_ota_example_task(void *pvParameter);
}

// global objects
EventGroupHandle_t event_group;
int64_t now; // milliseconds from startup
uint8_t Tread=1; // interval in seconds between temperature readings
uint8_t Tsendtemps=60; // interval in seconds between temperature transmissions to mqtt broker
uint8_t Ton=40; // On time in seconds of the pump (usually is the time required to empty the panel) 
uint8_t Toff=40; // Off time in seconds
uint8_t DT_TxMqtt=2; // if one of the two value of temperature red exceeds this delta, then values are transmitted to mqtt
uint8_t DT_ActPump=2; // if Tpanel > Ttank + DT_ActPump, then pump is acted
char *otaurl; // otaurl https://otaserver:8070/SolarThermostat.bin
bool forcePumpOn=false; // flag to force pump on
bool disableThermostat=false;



TempSens panelTemp;
TempSens tankTemp;
Switch solarPump(GPIO_PUMP);
Switch statusLed(GPIO_LED);
BinarySensor pushButton(GPIO_BUTTON,GPIO_PULLDOWN_ONLY);

WiFi wifi;
NvsParameters param;
Otafw otafw;


    
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_crt_end[]   asm("_binary_ca_crt_end");

void onNewCommand(char *s);

{
    switch (event->event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt->Subscribe(MqttControlTopic);
            mqtt->Subscribe(solarPump.commandTopic);
            if(disableThermostat)
                mqtt->Publish(MqttStatusTopic,"OFF");
            else
                mqtt->Publish(MqttStatusTopic,"ON");
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
            solarPump.newMqttMsg(event->topic,event->data);
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
            //statusLed.tOff=5000;
            //mqtt.Stop();
            break;
        case WIFI_GOT_IP: // connected
            ESP_LOGI(TAG,"GotIP");
            //ESP_LOGI("Connected. ip=%s",wifi->ip);
            xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
            //statusLed.tOff=1000;
            //mqtt.Start();
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


    // wifi ssid
    if (strcmp(token,"wifissid")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("wifi_ssid",token);
            return;
        }
    }

    // wifi password
    if (strcmp(token,"wifipassword")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            param.save("wifi_password",token);
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

    // on
    if (strcmp(token,"pumpon")==0)
    {
        forcePumpOn=true;
        return;
    }
    if (strcmp(token,"ON")==0)
    {
        disableThermostat=false;
        mqtt.Publish(MqttStatusTopic,"ON");
        return;
    }
    if (strcmp(token,"OFF")==0)
    {
        disableThermostat=true;
        mqtt.Publish(MqttStatusTopic,"OFF");
        return;
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


void ProcessStdin() {
    static char cmd[MAXCMDLEN];
    static uint8_t cmdlen=0;
    int c = fgetc(stdin);
    if(c!= EOF) 
    {
        printf("%c",c);
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

void setGlobalVars()
{
    panelTemp.start(GPIO_TEMPSENS);
    tankTemp.start(GPIO_TEMPSENS);

}
void app_main(void)
{
    setGlobalVars();
    loadParams();
    startThermostatTask();
    initWifi();
    startOtaTask();

    param.Init();

    event_group = xEventGroupCreate();

    // setup log
    //esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("mqtt_client", ESP_LOG_ERROR);
    //esp_log_level_set("main", ESP_LOG_DEBUG);
    esp_log_level_set("Switch", ESP_LOG_DEBUG);
    

    // status led blink fast (no wifi)
    statusLed.tOn=4000;
    statusLed.tOff=4000;

    //configure solarPump mqtt switch
    solarPump.tOn=Ton*1000;
    solarPump.tOff=Toff*1000;
    if(param.load("Ton",&solarPump.tOn)!=ESP_OK) solarPump.tOn=10000;
    if(param.load("Toff",&solarPump.tOff)!=ESP_OK) solarPump.tOff=10000;

    // configure wifi
    char *ssid=NULL;
    char *password=NULL;
    param.load("wifi_ssid",&ssid,"ssid");
    param.load("wifi_password",&password,"password");
    wifi.onEvent=&WiFiEvent;
    if(ssid) wifi.Start(ssid,password);
    free(ssid);
    free(password);

    // configure panelTemp mqtt sensor
    panelTemp.minIntervalBetweenMqttUpdate=Tsendtemps;
    param.load("Tsendtemps",&panelTemp.minIntervalBetweenMqttUpdate);
    param.load("DT_TxMqtt",&panelTemp.minVariationBetweenMqttUpdate);

    // configure tankTemp mqtt sensor
    tankTemp.minIntervalBetweenMqttUpdate=panelTemp.minIntervalBetweenMqttUpdate;
    tankTemp.minVariationBetweenMqttUpdate=panelTemp.minVariationBetweenMqttUpdate;
    
    //setup ota
    param.load("otaurl",&otaurl);
    if(otaurl) 
    {
        otafw.Init(otaurl,(const char*)ca_crt_start);
        xTaskCreate(&Ota, "ota_task", 8192, NULL, 5, NULL);
        ESP_LOGI(TAG,"Ota started");
    }

    // load various params

    param.load("Tread",&Tread);
    param.load("DT_ActPump",&DT_ActPump);

    // configure ds18b20s


    ESP_LOGI(TAG,"Starting. Version=%u Tread=%u Tsendtemps=%u Ton=%lu Toff=%lu DT_TxMqtt=%u DT_ActPump=%u",VERSION,Tread,Tsendtemps,solarPump.tOn/1000,solarPump.tOff/1000,DT_TxMqtt,DT_ActPump);
    xTaskCreate(&Ota, "ota_task", 8192, NULL, 5, NULL);
    /*
        Main cycle:
        periodically (Tpoll) read temperatures Tp and Tt and act the pump if necessary
        periodically (Tsendtemps) send Tp and Tt to MQTT broker, but only if they differ more than a certain amount (deltaT)
        read characters from stdin and respond to commands issued
    */
}


void searchTempSensor() {
    bus[0].start((gpio_num_t)GPIO_SENS_PANEL);
    bus[1].start((gpio_num_t)GPIO_SENS_TANK);
    vTaskDelay(1);
    for(uint8_t i=0;i<2;i++) {
        bus[i].search_all();
        ESP_LOGI(TAG,"bus %u, found: %u sensor(s)\n",i,bus[i].devices);
        if(bus[i].devices>0) {
            bus[i].setResolution(0,10);
        }
    }
}




void ttask() 
{
    TTimer timerReadTemp;
    timerReadTemp.value=Tread;

    while(true)
    {
        if(timerReadTemp.elapsed) {
            ReadAndSendTemperatures(sens);
        }
        ProcessThermostat(sens);
        vTaskDelay(100);
    }
}

void ReadAndSendTemperatures(sens)
{
    if(panelTemp.Read())
    {
        if(panelTemp.mustSignal()) 
        {
            485.SignalPanelTemp(panelTemp.value);
            panelTemp.Signaled();
        }
    }
    tankTemp.Read();
}

void ProcessThermostat() 
{
    pushButton.run();
    cond=(panelTemp.value > tankTemp.value + DT_ActPump) || pushButton.state;
    ESP_LOGI(TAG,"cond=%d butt=%d",cond,pushButton.state);
    solarPump.run(cond);
    statusLed.run(true); // flash  
}


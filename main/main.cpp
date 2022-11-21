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
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "driver/gpio.h"
// https://github.com/UncleRus/esp-idf-lib
// https://esp-idf-lib.readthedocs.io/en/latest/groups/ds18x20.html
#include <ds18x20.h>
#include "otafw.h"
//#include <iostream>


#include "wifi.h"
#include "mqtt.h"
#include "nvsparameters.h"
#define MAXCMDLEN 200
#define TOTACheck 24
#define WIFI_CONNECTED_BIT BIT0
#define OTA_BIT      BIT1
// ds18b20 sensors
#define GPIO_SENS_PANEL GPIO_NUM_21
#define GPIO_SENS_TANK GPIO_NUM_23
#define GPIO_FP_SENS GPIO_NUM_25
// pump actuators
#define GPIO_PUMP GPIO_NUM_18
#define GPIO_FPPUMP GPIO_NUM_32
#define GPIO_TANKPUMP GPIO_NUM_33
// binary sensors
#define GPIO_FLUX GPIO_NUM_34
#define GPIO_BOILERPUMP GPIO_NUM_35

#define GPIO_LED GPIO_NUM_4
#define GPIO_BUTTON GPIO_NUM_2
#define VERSION 10

static const char *TAG = "main";

extern "C" {
    void app_main(void);
    extern void simple_ota_example_task(void *pvParameter);
}

// global objects
EventGroupHandle_t event_group;
float Tp=0,Tt=0; // store the last temp read
int64_t now; // milliseconds from startup
uint8_t Tread=5; // interval in seconds between temperature readings
uint8_t Tsendtemps=1; // interval in minutes between temperature transmissions to mqtt broker
uint8_t Ton=40; // On time in seconds of the pump (usually is the time required to empty the panel) 
uint8_t Toff=40; // Off time in seconds
uint8_t SecFpPumpOn=30; // num of seconds fp pump must stays on
uint8_t DT_TxMqtt=2; // if one of the two value of temperature red exceeds this delta, then values are transmitted to mqtt
uint8_t DT_ActPump=2; // if Tpanel > Ttank + DT_ActPump, then pump is acted
uint8_t MinTankTempToUseForWaterHeathing=30;
bool BoilerPump; // true if boiler pump is on
bool FluxSensor; // true if water flux is detected
char *MqttTpTopic; //  mqtt_tptopic SolarThermostat/Tp
char *MqttTtTopic; //  mqtt_tttopic SolarThermostat/Tt
char *MqttControlTopic; // mqtt_cttopic SolarThermostat/control
char *MqttStatusTopic; // SolarThermostat/status
char *otaurl; // otaurl https://otaserver:8070/SolarThermostat.bin
bool forcePumpOn=false; // flag to force pump on
bool disableThermostat=false;

Mqtt mqtt;
WiFi wifi;
NvsParameters param;
Otafw otafw;
ds18x20_addr_t panel_sens[1];
ds18x20_addr_t tank_sens[1];
extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");

void onNewCommand(char *s);
char* toLower(char* s);

void MqttEvent(Mqtt* mqtt, esp_mqtt_event_handle_t event)
{
    switch (event->event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt->Subscribe(MqttControlTopic);
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
            //mqtt.Stop();
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
    char *token = toLower(strtok(s, delim));
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
    if (strcmp(token,"on")==0)
    {
        disableThermostat=false;
        mqtt.Publish(MqttStatusTopic,"ON");
        return;
    }
    if (strcmp(token,"off")==0)
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



    if (strcmp(token,"secfppumpon")==0)
    {
        token = strtok(NULL, delim);
        if(token==NULL) err=1;
        if(err==0) {
            int j=atoi(token);
            if(j<1 || j>256) err=2;
            if(err==0) {
                SecFpPumpOn=j;
                param.save("SecFpPumpOn",j);
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

void ProcessThermostatSolar() {
    static int64_t tchange=0;
    static bool needPumpprec=false;
    static uint8_t state=0;
    bool needPump=(Tp > Tt + DT_ActPump); // condition that pump action is needed
    bool condA = ((needPump==true) && (needPumpprec==false) && (state==0)); 
    bool condB = ((state==0) && (needPump==true) && (((now - tchange)/1000) >= Toff));
    bool condC = ((state==1) && (((now - tchange)/1000) >= Ton));
    if(forcePumpOn) {condA = true; forcePumpOn=false;}
    ESP_LOGD(TAG, "cond np,a,b,c: %u,%u,%u,%u - state:%u",needPump,condA,condB,condC,state);
    if( (condA || condB) && !disableThermostat ) {
        state=1;
        gpio_set_level(GPIO_PUMP,1);
        tchange=now;
        ESP_LOGD(TAG,"pump on");
    }
    if( condC || disableThermostat) {
        state=0;
        gpio_set_level(GPIO_PUMP,0);
        tchange=now;
        ESP_LOGD(TAG,"pump off");
    }
    needPumpprec=needPump;

}
void ProcessThermostatFirePlace() {
    static uint8_t state=-1;
    static int64_t tchange=0;

    bool conditionOff =  (((now - tchange)/1000) >= SecFpPumpOn);
    if(state== -1 || (state==1 && conditionOff)) {
        state=0;
        gpio_set_level(GPIO_FPPUMP,0);
    }

    bool conditionOn=(Tf>ThTFp); // threshold temp fireplace
    if(state==0 && conditionOn) {
        state=1;
        tchange=now;
        gpio_set_level(GPIO_FPPUMP,1);
    }

    //ESP_LOGD(TAG, "cond np,a,b,c: %u,%u,%u,%u - state:%u",needPump,condA,condB,condC,state);

}
void ProcessThermostatTank() {
    static uint8_t state=-1;
    static int64_t tchange=0;

    if(state== -1 || (state==1 && !conditionOn)) {
        state=0;
        gpio_set_level(GPIO_TANKPUMP,0);
    }

    bool conditionOn=((Tt>MinTankTempToUseForWaterHeathing) && (FluxSensor==1 || BoilerPump==1)); // threshold temp fireplace
    if(state==0 && conditionOn) {
        state=1;
        tchange=now;
        gpio_set_level(GPIO_TANKPUMP,1);
    }
}

/* void SensorError(int sensorpin, int funcerr)
{
    if(!disableThermostat) {
        mqtt.Publish(MqttStatusTopic,"OFF");
        char msg[50];
        sprintf(msg,"Sens fault %u %u",sensorpin,funcerr);
        ESP_LOGI(TAG, "%s", msg);
        mqtt.Publish(MqttStatusTopic,msg);
        disableThermostat = true;
    }
} */

bool ReadTemperatures() {
    float ttmp;
    
    if(ds18s20_measure_and_read(GPIO_SENS_PANEL, DS18X20_ANY, &ttmp))
        Tp=ttmp;
    vTaskDelay(1);


    if(ds18s20_measure_and_read(GPIO_SENS_TANK, DS18X20_ANY, &ttmp)
        Tt=ttmp;
    vTaskDelay(1);
    
    if(ds18s20_measure_and_read(GPIO_FP_SENS, DS18X20_ANY, &ttmp))
        Tf=ttmp;
    vTaskDelay(1);


    ESP_LOGD(TAG, "Tp, Tt, Tf: %.1f,%.1f,%.1f",Tp,Tt,Tf);
    return true;
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


void app_main(void)
{
    param.Init();
    int64_t tlastread=0;  // time of the last temperatures read
    int64_t tlastsenttemp=0;  // time of the last temperatures send
    int64_t tlastotacheck=0;
    int64_t tlastpt=0;
    float Tplast=0; // previous reading of Tp
    float Ttlast=0; // previous reading of Tt

    //srand((unsigned int)esp_timer_get_time());
    event_group = xEventGroupCreate();

    // setup gpio
    gpio_set_direction(GPIO_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_FP_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_TANK_PUMP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_BOILERPUMP, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_FLUX, GPIO_MODE_INPUT);
    

    gpio_pullup_en(GPIO_BUTTON);
    gpio_pullup_en(GPIO_FLUXSENS);
    gpio_pullup_en(GPIO_HEATPUMP);
    gpio_set_level(GPIO_LED,0);
    gpio_set_level(GPIO_PUMP,0);
    gpio_set_level(GPIO_TANK_PUMP,0);
    gpio_set_level(GPIO_FP_PUMP,0);
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    // configure wifi
    char *ssid=NULL;
    char *password=NULL;
    param.load("wifi_ssid",&ssid,"ssid");
    param.load("wifi_password",&password,"password");
    wifi.onEvent=&WiFiEvent;
    if(ssid) wifi.Start(ssid,password);
    free(ssid);
    free(password);
    //configure mqtt
    char *username=NULL;
    password=NULL;
    char *uri=NULL; // mqtturi mqtts://mqttserver:8883
    param.load("mqtt_username",&username);
    param.load("mqtt_password",&password);
    param.load("mqtt_uri",&uri);
    param.load("mqtt_tptopic",&MqttTpTopic,"SolarThermostat/Tp");
    param.load("mqtt_tttopic",&MqttTtTopic,"SolarThermostat/Tt");
    param.load("mqtt_cttopic",&MqttControlTopic,"SolarThermostat/control");
    param.load("mqtt_sttopic",&MqttStatusTopic,"SolarThermostat/status");
    if(uri) 
    {
        mqtt.Init(username,password,uri,(const char*)ca_crt_start);
        mqtt.onEvent=&MqttEvent;
        ESP_LOGI(TAG,"Mqtt started");
    }
    free(username);
    free(password);
    free(uri);

    param.load("otaurl",&otaurl);
    if(otaurl) 
    {
        otafw.Init(otaurl,(const char*)ca_crt_start);
        xTaskCreate(&Ota, "ota_task", 8192, NULL, 5, NULL);
        ESP_LOGI(TAG,"Ota started");
    }

    param.load("Tread",&Tread);
    param.load("Tsendtemps",&Tsendtemps);
    param.load("Ton",&Ton);
    param.load("Toff",&Toff);
    param.load("DT_TxMqtt",&DT_TxMqtt);
    param.load("DT_ActPump",&DT_ActPump);
    param.load("SecFpPumpOn",&SecFpPumpOn);
    param.load("MinTankTempToUseForWaterHeathing",&MinTankTempToUseForWaterHeathing);

    
    // configure ds18b20s
/*     size_t sensor_count;
    esp_err_t err;
    err=ds18x20_scan_devices(GPIO_SENS_PANEL, panel_sens,1, &sensor_count);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"Panel sensor scan failed");
        SensorError(GPIO_SENS_PANEL,2);
    } else
    {
        if (sensor_count < 1) {
            ESP_LOGE(TAG,"Panel sensor not detected");
            SensorError(GPIO_SENS_PANEL,3);
        }
    }
    
    err=ds18x20_scan_devices(GPIO_SENS_TANK, tank_sens,1, &sensor_count);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"Tank sensor scan failed");
        SensorError(GPIO_SENS_TANK,2);
    } else
    {
        if (sensor_count < 1) {
            ESP_LOGE(TAG,"Tank sensor not detected");
            SensorError(GPIO_SENS_TANK,3);
        }
    }

    err=ds18x20_scan_devices(GPIO_FP_SENS, fp_sens,1, &sensor_count);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG,"Tank sensor scan failed");
        SensorError(GPIO_SENS_TANK,2);
    } else
    {
        if (sensor_count < 1) {
            ESP_LOGE(TAG,"FP sensor not detected");
            SensorError(GPIO_SENS_TANK,4);
        }
    }

 */    
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
        FluxSensor=gpio_get_level(GPIO_FLUX);
        BoilerPump=gpio_get_level(GPIO_BOILERPUMP);
        // reschedule a otafw check after TOTACheck hours
        if(((now - tlastotacheck)/3600000) >= TOTACheck) {
            xEventGroupSetBits(event_group,OTA_BIT); // force a ota check every TOTACheck hours
            tlastotacheck=now;
        }
        // process commands from stdin
        ProcessStdin();   
        if(gpio_get_level(GPIO_BUTTON)==1) forcePumpOn=true;
        // read temperatures every Tread 
        if((now - tlastread) >= Tread) {
            tlastread=now;
            char msg[10];
            ReadTemperatures();
        
            // send to mqtt broker if it's the case
            if(((now - tlastsenttemp)/60000) >= Tsendtemps) {
                if( abs(Tp-Tplast) >= DT_TxMqtt || abs(Tt-Ttlast) >= DT_TxMqtt || abs(Tf-Tflast) >= DT_TxMqtt) {
                    sprintf(msg,"%.1f",Tp);
                    mqtt.Publish(MqttTpTopic,msg);
                    sprintf(msg,"%.1f",Tt);
                    mqtt.Publish(MqttTtTopic,msg);
                    sprintf(msg,"%.1f",Tf);
                    mqtt.Publish(MqttTfTopic,msg);
                    Tplast=Tp;
                    Ttlast=Tt;
                    Tflast=Tf;
                    tlastsenttemp=now;
                }
            }

        }
        // process thermostat every second
        if( (now - tlastpt) >= 1000) {
            ProcessThermostatSolar();
            ProcessThermostatFirePlace();
            ProcessThermostatTank();
            tlastpt=now;
        }
        vTaskDelay(1);
    }
}

void publishStatus() {
    sprintf(msg,"%.1f",Tp);
                    mqtt.Publish(MqttTpTopic,msg);
    mqtt.Publish(MqttStatusTopic,"OFF");
}



char* toLower(char* s) {
  for(char *p=s; *p; p++) *p=tolower(*p);
  return s;
}
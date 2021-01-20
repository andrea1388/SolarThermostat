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

// https://github.com/UncleRus/esp-idf-lib
// https://esp-idf-lib.readthedocs.io/en/latest/groups/ds18x20.html
#include <ds18x20.h>

#define GPIO_SENS_PANEL 1
#define GPIO_SENS_TANK 2


extern void initGPIO();
extern void loadParameters();
extern void wifi_init_sta();
extern void simple_ota_example_task(void *pvParameter);
extern void mqtt_app_start(void);

static EventGroupHandle_t s_wifi_event_group;

void app_main(void)
{
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
    
    float Tp,Tt;
    while(true) {
        vTaskDelay(1);
        //fgets(url_buf, OTA_URL_SIZE, stdin);
        ds18x20_measure(GPIO_SENS_PANEL, ds18x20_ANY, false);
        ds18x20_measure(GPIO_SENS_TANK, ds18x20_ANY, false);
        vTaskDelay(1);
        ds18x20_read_temperature(GPIO_SENS_PANEL, ds18x20_ANY, &Tp);
        ds18x20_read_temperature(GPIO_SENS_TANK, ds18x20_ANY, &Tt);

            
    }
}




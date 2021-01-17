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
void initGPIO();
void loadParameters();
void wifi_init_sta();

static EventGroupHandle_t s_wifi_event_group;

void app_main(void)
{
    loadParameters();
    initGPIO();
    wifi_init_sta();
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 5, NULL);
    while(true) {
        vTaskDelay(1);
        fgets(url_buf, OTA_URL_SIZE, stdin);
            
    }
}

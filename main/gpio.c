// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html

#include "driver/gpio.h"
#define GPIOPUMP 18
void initGPIO() 
{
    gpio_pad_select_gpio(GPIOPUMP);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(GPIOPUMP, GPIO_MODE_INPUT);
}

void pumpOnOff(bool on)
{
    if(on) gpio_set_level(GPIOPUMP,1); else gpio_set_level(GPIOPUMP,0);
}


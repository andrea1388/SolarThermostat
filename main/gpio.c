// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html

#include "driver/gpio.h"

inline void initGPIO() 
{
    gpio_pad_select_gpio(18);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(18, GPIO_MODE_INPUT);
}
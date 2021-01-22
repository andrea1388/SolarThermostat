// https://github.com/UncleRus/esp-idf-lib
// https://esp-idf-lib.readthedocs.io/en/latest/groups/ds18x20.html
#include <ds18x20.h>
extern float Tp,Tt;
inline ReadTemperatures() {
    ds18x20_measure(GPIO_SENS_PANEL, ds18x20_ANY, false);
    ds18x20_measure(GPIO_SENS_TANK, ds18x20_ANY, false);
    vTaskDelay(1);
    ds18x20_read_temperature(GPIO_SENS_PANEL, ds18x20_ANY, &Tp);
    ds18x20_read_temperature(GPIO_SENS_TANK, ds18x20_ANY, &Tt);

}
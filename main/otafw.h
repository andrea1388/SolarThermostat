#ifndef otafw_hpp
#define otafw_hpp

#include "esp_https_ota.h"
#include <esp_ota_ops.h>
#include "esp_http_client.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
/**
 * @brief Wrapper class for ota fw update
 */

class Otafw
{
public:
    void Init(const char *url, const char *cert);
    void Check();


private:
    esp_https_ota_config_t ota_config;
    esp_http_client_config_t http_config;
    static esp_err_t handler(esp_http_client_event_t *evt);
};
#endif

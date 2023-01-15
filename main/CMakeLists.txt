idf_component_register(SRCS 
"ota.cpp"
"main.cpp"
INCLUDE_DIRS ""
REQUIRES MqttSensors app_update esp_https_ota esp_timer driver Esp32MqttComponent Esp32NvsComponent Esp32WiFiComponent Esp32Ds18b20Component
)
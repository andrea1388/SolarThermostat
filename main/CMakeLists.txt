idf_component_register(SRCS 
"ota.cpp"
"main.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
REQUIRES app_update esp_https_ota esp_timer driver Esp32MqttComponent Esp32NvsComponent Esp32WiFiComponent
)
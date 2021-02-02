idf_component_register(SRCS 
"main.cpp" 
"loadParameters.c" 
"ota.c"
"mqtt.c" 
"wifi.cpp"
"cmdDecoder.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
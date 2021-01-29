idf_component_register(SRCS 
"main.cpp" 
"loadParameters.c" 
"ota.c"
"mqtt.c" 
"wifi.c"
"cmdDecoder.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
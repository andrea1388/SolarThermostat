idf_component_register(SRCS 
"main.c" 
"loadParameters.c" 
"ota.c"
"mqtt.c" 
"wifi.c"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
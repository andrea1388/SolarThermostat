idf_component_register(SRCS 
"ota.c"
"cmdDecoder.cpp"
"main.cpp"
"mqtt.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
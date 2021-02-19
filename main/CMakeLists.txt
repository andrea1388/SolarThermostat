idf_component_register(SRCS 
"main.cpp" 
"loadParameters.c" 
"ota.c"
"cmdDecoder.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
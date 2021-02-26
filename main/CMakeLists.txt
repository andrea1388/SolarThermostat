idf_component_register(SRCS 
"ota.c"
"main.cpp"
INCLUDE_DIRS ""
EMBED_TXTFILES ${project_dir}/certs/ca.crt
)
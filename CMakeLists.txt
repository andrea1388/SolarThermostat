# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
# cmake_minimum_required(VERSION 3.11)
cmake_minimum_required(VERSION 3.16)




# set(EXTRA_COMPONENT_DIRS ${espidflib_SOURCE_DIR}/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SolarThermostat)
target_add_binary_data(SolarThermostat.elf "certs/ca.crt" TEXT)

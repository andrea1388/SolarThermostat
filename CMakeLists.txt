# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.11)

include(FetchContent)
FetchContent_Declare(
  espidflib
  GIT_REPOSITORY https://github.com/UncleRus/esp-idf-lib.git
)

FetchContent_Declare(
  Esp32MqttComponent
  GIT_REPOSITORY https://github.com/andrea1388/Esp32MqttComponent.git
)

FetchContent_Declare(
  Esp32NvsComponent
  GIT_REPOSITORY https://github.com/andrea1388/Esp32NvsComponent.git
)

FetchContent_Declare(
  Esp32WiFiComponent
  GIT_REPOSITORY https://github.com/andrea1388/Esp32WiFiComponent.git
)

FetchContent_MakeAvailable(espidflib Esp32MqttComponent Esp32NvsComponent Esp32WiFiComponent)
set(EXTRA_COMPONENT_DIRS ${espidflib_SOURCE_DIR}/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SolarThermostat)
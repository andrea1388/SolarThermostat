# SolarThermostat
OTA MQTT Solar Heater Controller (Thermostat) - Differential Temperature Controller for Solar Hot Water

ESP32 App(lication) for controlling the pump of a solar plant.
It checks the temperature of the solar panel (Ts), and compares it with the temperature of the tank (Tt). If Ts>Tt acts the pump with an optional duty cicle.
Uses MQTT to report temperature values. I plan to use it with the Home Assistant controller. Uses OTA for firmware update over the air.

Andrea Carrara

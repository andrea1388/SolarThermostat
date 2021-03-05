cp ../build/SolarThermostat.bin .
openssl s_server -WWW -key ca.key -cert ca.crt -port 8070

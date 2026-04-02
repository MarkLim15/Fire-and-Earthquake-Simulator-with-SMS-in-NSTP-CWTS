A multi-board embedded system built with Arduino Mega, Arduino Uno, ESP8266, and ESP32
for simulating and detecting earthquake and fire events.

## Boards
- mega_receiver — Main controller (LCD, relays, speaker, lights)
- mega_earthquake — Accelerometer + motor control
- uno_fire_sensor — IR flame sensor node
- esp8266_sms — GSM SMS alerts via SIM900A
- esp32_iot — Firebase + ThingSpeak IoT dashboard

## Components Used
- Arduino Mega x2, Arduino Uno, ESP8266 NodeMCU, ESP32
- ADXL335 Accelerometer, IR Flame Sensors x5
- SIM900A GSM Module, 16x2 I2C LCD, DC Motor, Relay Modules

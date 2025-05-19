# IoT Prototype for monitoring water parameters and automatically controlling a valve

This repository contains code, research and schematics to use the following components connected to an esp32 devkit:

- pH sensor PH4502C
- Waterproof 1 Wire digital temperature sensor DS18B20
- Turbidity sensor MJKDZ
- Ultrassonic distance sensor HC-SR04
- 3V 1 channel relay to toggle a 127V AC solenoid valve (close/open water passage)

When any of the sensors report values outside of configurable thresholds, the valve will close or open according to configuration. Check the start of `main.ino` to edit in your preferences.

# How to build for esp32 DevKit C v4

1. In VS Code, [install `PlarformIO IDE` extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
2. Connect your esp32 dev kit to the computer using an usb port. [You might need drivers for your USB controller integrated in your devkit, for example these for CP210x. Get the latest one.](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads)
3. In `main.ino`, place your values for wifi ssid and password, change clientId to any random unique string. Change the parameter thresholds and valve action if desired.
4. On the PlatformIO tab to the left, in `project tasks` > `esp32dev` > `General` click `Upload and Monitor` to build the project, flash it to the esp32 and begin monitoring.

The code is compatible with other platforms, go to Platform IO documentation for how to use them, or just take `main.ino` and use it in Arduino IDE, for example. The `esp32dev` Platform IO target is suitable for multiple devkits by expressif powered by a esp32 module.

# Wokwi simulation and schema for connections
https://wokwi.com/projects/429623946087821313


# Video demonstration

To be uploaded
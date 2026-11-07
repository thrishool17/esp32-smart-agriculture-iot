# IoT-Based Smart Agriculture Environmental Monitoring System using ESP32

[[https://img.shields.io/badge/PlatformIO-Supported-orange.svg]](https://platformio.org/)
[[https://img.shields.io/badge/Framework-Arduino-blue.svg]](https://www.arduino.cc/)
[[https://img.shields.io/badge/Microcontroller-ESP32%20DevKit-red.svg]](https://www.espressif.com/)
[[https://img.shields.io/badge/Cloud-ThingSpeak%20IoT-teal.svg]](https://thingspeak.com/)
[[https://img.shields.io/badge/License-MIT-yellow.svg]](https://opensource.org/licenses/MIT)

An automated, low-power **IoT Smart Agriculture Field Station** built on the **ESP32 microcontroller**. The system monitors real-time environmental and soil parameters—including **soil moisture**, **ambient temperature**, **relative humidity**, and **atmospheric gas concentrations**—displaying telemetry locally on a 0.96" SSD1306 OLED display and streaming time-series data to the **ThingSpeak IoT Cloud**.

### Live Dashboard
<img width="1920" height="1200" alt="Screenshot (637)" src="https://github.com/user-attachments/assets/5f00d8f5-87c6-4e37-95a5-203fe9a5e2ad" />
[hardware.jpg]
<img width="3072" height="4096" alt="IMG_20261107_131615" src="https://github.com/user-attachments/assets/e9a46584-e1b8-4de5-a957-c54a6eaafb7c" />



## Hardware Components & Pinout

| Component | Pin on Component | ESP32 GPIO | Operating Voltage | Function |
| :--- | :--- | :--- | :--- | :--- |
| **Capacitive Soil Moisture** | AOUT | **GPIO 34** | 3.3V | Soil moisture analog input (ADC1) |
| **DHT11 Climate Sensor** | S (Signal) | **GPIO 4** | 3.3V | Temperature & relative humidity |
| **MQ Gas Sensor (FC-22)** | AO (Pin 1) | **GPIO 35** | 5V (VIN) | Air quality & gas level analog input (ADC1) |
| **0.96" I2C OLED (SSD1306)** | SDA / SCL | **GPIO 21 / 22** | 3.3V | Local 128x64 display |

---

## Features
- **Non-blocking task scheduling** using `millis()` (sensor refresh every 2s, cloud push every 20s).
- **Dual Display**: Onboard 0.96" OLED screen and embedded local web dashboard table at `http://<ESP32_IP>/`.
- **Cloud Telemetry**: Automated channel logging on ThingSpeak with live time-series charts.
- **Hardware-safe ADC routing**: Analog sensors assigned strictly to **ADC1** pins (`GPIO 34`, `GPIO 35`) to prevent conflict with the active Wi-Fi radio.

---


## Author

- **GitHub:** [@thrishool17](https://github.com/thrishool17)

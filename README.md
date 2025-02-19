## Overview
This project utilizes the Nordic nRF52840 and various sensors for data collection and analysis. Below are the sensors used, their datasheets, and their respective pin connections.

## Sensors and Connections

### On-Board Sensors (Connected to PCB)

1. **MLX90614 (Infrared Temperature Sensor)**  
   - [Datasheet](https://www.melexis.com/en/documents/documentation/datasheets/datasheet-mlx90614)  
   - **Connections:**
     - SDA -> P0.13
     - SCL -> P0.15
     - VDD -> VDD
     - GND -> GND

2. **MPU6050 (Accelerometer & Gyroscope)**  
   - [Datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf)  
   - **Connections:**
     - SDA -> P0.13
     - SCL -> P0.15
     - VDD -> VDD
     - GND -> GND

3. **BMP280 (Barometric Pressure Sensor)**  
   - [Datasheet](https://cdn-shop.adafruit.com/datasheets/BST-BMP280-DS001-11.pdf)   
   - **Connections:**
     - SDI -> P0.13
     - SCK -> P0.15
     - SDO -> VDD
     - 3Vo -> VDD
     - GND -> GND

### External Sensors (Connected Externally)

4. **MAX30102 (Heart Rate & Blood Oxygen Sensor)**  
   - [Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max30102.pdf)  
   - **Connections:**
     - SDA -> P1.10
     - SCL -> P1.15
     - VDD -> VDD
     - GND -> GND

5. **SEN-11754 (Pulse Sensor)**  
   - [Datasheet](https://www.digikey.com/en/products/detail/sparkfun-electronics/SEN-11574/5762397)  
   - **Connections:**
     - ADC -> P0.29
     - VDD -> VDD
     - GND -> GND

6. **Conductive Rubber Cord (Stretch Sensor)**  
   - [Datasheet](https://www.verical.com/datasheet/adafruit-misc-sensors-519-5047094.pdf)  
   - **Connections:**
     - ADC -> P0.31
     - VDD -> VDD
     - GND -> GND

## Notes
- Ensure pull-up resistors are used where necessary for I2C lines.
- ADC: AIN5 - 0.29 (Pulse Sensor), AIN7 - 0.31 (Rubber Cord)
- LEDs: R - 0.02, G - 0.22, B - 0.24


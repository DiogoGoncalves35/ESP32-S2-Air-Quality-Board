# ESP32-S2 Air Quality Board

An IoT Air Quality Monitor

![Arduino](https://img.shields.io/badge/Arduino-00878F.svg?style=for-the-badge&logo=Arduino&logoColor=white) ![PlatformIO](https://img.shields.io/badge/PlatformIO-F5822A.svg?style=for-the-badge&logo=PlatformIO&logoColor=white) ![Kicad](https://img.shields.io/badge/KiCad-314CB0.svg?style=for-the-badge&logo=KiCad&logoColor=white) ![MQTT](https://img.shields.io/badge/MQTT-660066.svg?style=for-the-badge&logo=MQTT&logoColor=white) ![Home Assistant](https://img.shields.io/badge/home%20assistant-%2341BDF5.svg?style=for-the-badge&logo=home-assistant&logoColor=white) 


This board is designed to measure TVOC (Total Volatile Organic Compounds), eCO2(Estimated Concentration of Carbon Dioxide) and calculate AQI (Air Quality Index) of the ambient arround. 

<img src="/Images/Board.jpg">

It's a very simple design, it was designed with the intetion of testing ENS160 + AHT21 module. 
In the end AHT21 sensor is not used because ENS160 has a heater and influences temperature readings, values are about 5 degrees higher then they shoud be.

At the moment the LVGL interface only shows eCO2 and AQI values, there is no information about TVOC, Wi-Fi and MQTT status.

------------

### Home Assistant

It supports Home Assistant via HA MQTT Auto Discovery,  there is no need to integrate this device via web interface or yaml code, it's a faster solution to integrate devices. 
The microcontroller sends TVOC, eCO2 and AQI values, and also a "On" message. Values are send every 15 seconds and if there is no value recevied 35 seconds after the last, HA assumes that the device is offline.

<img src="/Images/HA-MQTT.png">

------------

### Schematic

The [schematic](/PCB/Schematic.pdf) is designed arround ESP32-S2 Mini Board, LCD connects via SPI and ENS160 with I²C. Board can be powered with 5V from USB connector or via ESP32-S2 Mini Board USB connector. The AJ38 module step downs the voltage to 3.3V, and powers the components. A buzzer was added but at the moment is not in use.

<img src="/Images/Schematic.png">

------------

### PCB

[This PCB](/PCB) has a very simple design, it's a 1.6 mm board with 2 copper layers, both connect to GND. Next version should have a better design, the microcontroller must be at one edge, or the GND pour needs to be removed under, it's a good practice when there is an antenna.

<img src="/Images/PCB.png">

------------

### Code

Every line of [code](/Code/Air%20Quality%20Monitor) contains a commentary. Some libraries need a few changes.

Configuration for User_Setup.h (TFT_eSPI Library)
```arduino
#define ST7735_DRIVER

#define TFT_RGB_ORDER TFT_RGB
#define TFT_WIDTH  160
#define TFT_HEIGHT 128

#define TFT_MOSI  35
#define TFT_SCLK  36
#define TFT_CS    34  
#define TFT_DC    33  
#define TFT_RST   18
```


Configuration for lv_conf.h (lvgl Library)
```arduino
#if 1

#define LV_TICK_CUSTOM 1

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1

```

------------

### Known issues

- No I²C Pull-Up (Sometimes module is not detected)
- ESP32-S2 need serial in other to work (Without module not works)
- Mounting Holes connected to GND

------------

### To-Do

- Better hardware design
- Other sensor to compare
- ESP32-S3 WROOM (No more boards)
- New LVGL interface
- Add restart button via Home Assistant
- USB-C
- Maybe LCD without the module
- Control LCD led backlight with a mosfet/transistor




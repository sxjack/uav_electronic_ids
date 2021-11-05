# id_scanner

Basic scanner for the ESP32 that looks for both opendroneid and French WiFi signals.

Outputs JSON on the USB port and drives both text and graphical displays.

Developed and tested with SH1106 128x64 OLED and ST7735 160x128 TFT displays. 
The U8g2 library was used to drive the SH1106. 
The TFT_eSPI library was used to drive the ST7735.

Requires opendroneid.c, opendroneid.h, odid_wifi.h and wifi.c from https://github.com/opendroneid.

* Libraries
  * https://github.com/olikraus/u8g2
  * https://github.com/Bodmer/TFT_eSPI

* Hardware
  * https://uk.banggood.com/Geekcreit-ESP32-WiFi+bluetooth-Development-Board-Ultra-Low-Power-Consumption-Dual-Cores-Pins-Unsoldered-p-1214159.html
  * https://www.banggood.com/1_3-Inch-4Pin-White-OLED-LCD-Display-12864-IIC-I2C-Interface-Module-p-1067874.html
  * https://www.banggood.com/1_8-Inch-LCD-Screen-SPI-Serial-Port-Module-TFT-Color-Display-Touch-Screen-ST7735-p-1414465.html


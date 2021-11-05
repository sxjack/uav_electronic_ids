# ESP32 Direct Remote ID 

## Bill of Material

* ESP32 Dev Module, you need one that has RX2 & TX2 available.
* GPS module
* 5V PSU

The GPS module needs to be one that can be programmed to output NMEA GGA and RMC at 19200 baud. 
A link to one that I have used is below.

## Tools

* Arduino IDE

## Build

* Program the ESP32 with the bin file using its USB port.
* Connect a 5V supply to VIN (optional).
* Connect the GPS module to TX2 & RX2.

## Configuration

* Connect the ESP32 to a PC via its USB port and connect to it using a terminal program (115200,8,N,1).
* You should see some diagnostics when the ESP32 powers up and details of the fix once the GPS has one.
* The program will accept configuration commands over this link.
The command print will show you the configuration commands.
The numbers for the EU classes etc are as defined in opendroneid.h.

## Notes

* The command to write the bin file to the ESP32 will be something like:
> C:\Users\zzz\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\2.6.1/esptool.exe --chip esp32 --port COM22 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 C:\Users\zzz\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4/tools/partitions/boot_app0.bin 0x1000 C:\Users\zzz\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4/tools/sdk/bin/bootloader_qio_80m.bin 0x10000 test_utm.esp32.bin 0x8000 F:\zzz_tmp\arduino_build_697712/test_utm.ino.partitions.bin
* A long flash of the LED indicates that the ESP32 is getting data from the GPS.
The number of short flashes following the long one indicates the number of satellites.
Three flashes indicates 10+ satellites should be a good 3D fix.

## Resources

* [ESP32 Dev Board](https://www.banggood.com/Geekcreit-ESP32-WiFi+bluetooth-Development-Board-Ultra-Low-Power-Consumption-Dual-Cores-Pins-Unsoldered-p-1214159.html)
* [Geekcreit 5Hz GPS](https://www.banggood.com/1-5Hz-VK2828U7G5LF-TTL-Ublox-GPS-Module-With-Antenna-p-965540.html)
* [5V supply](https://www.banggood.com/AMS1117-5_0V-5V-DC-DC-Step-Down-Power-Supply-Module-Power-Buck-Board-LDO-800MA-p-1578743.html)

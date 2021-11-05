# id_open
An Arduino/ESP32 class to act as a wrapper around opendroneid.

Supports BLE 4, WiFi NAN and WiFi beacon.

Runs on a cheap ESP32 dev board.

Needs opendroneid.c, opendroneid.h, odid_wifi.h and wifi.c from [opendroneid](https://github.com/opendroneid/opendroneid-core-c/tree/master/libopendroneid) to be copied into the id_open directory.

Last tested with opendroneid release 1.0.
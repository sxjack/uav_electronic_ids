# id_open
An Arduino/ESP32 class to act as a wrapper around opendroneid.

Runs on a cheap ESP32 dev board.

Needs opendroneid.c, opendroneid.h and odid_wifi.h from [opendroneid](https://github.com/opendroneid/opendroneid-core-c/tree/master/libopendroneid) to be copied into the id_open directory. Do not copy wifi.c, the wifi.c in this directory is slightly modified from the one in opendroneid.

Last tested with opendroneid release 1.0.
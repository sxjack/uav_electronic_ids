/* -*- tab-width: 2; mode: c; -*-
 * 
 * C++ class for Arduino to function as a wrapper around opendroneid.
 * This file has the ESP8266 specific code.
 *
 * Copyright (c) 2022, Steve Jack.
 *
 * Nov. '22:  Split out from id_open.cpp. 
 *
 * MIT licence.
 *
 * NOTES
 *
 * 
 */

#define DIAGNOSTICS 0

//

#if defined(ARDUINO_ARCH_ESP8266)

#pragma GCC diagnostic warning "-Wunused-variable"

#include <Arduino.h>
#include <sys/time.h>

#include "id_open.h"

#if ID_OD_WIFI 

#include <ESP8266WiFi.h>

extern "C" {
  int wifi_send_pkt_freedom(uint8 *,int,bool);
}

static Stream *Debug_Serial = NULL;

#endif // WIFI

/*
 *
 */

void construct2() {

  return;
}

/*
 *
 */

void init2(char *ssid,int ssid_length,uint8_t *WiFi_mac_addr,uint8_t wifi_channel) {

  char text[128];

  text[0] = text[63] = 0;

#if DIAGNOSTICS
  Debug_Serial = &Serial;
#endif

#if ID_OD_WIFI

  softap_config wifi_config;
  
  WiFi.mode(WIFI_OFF);

  WiFi.macAddress(WiFi_mac_addr);
  
  WiFi.softAP(ssid,NULL,wifi_channel,false,0);
  WiFi.setOutputPower(20.0);

  wifi_softap_get_config(&wifi_config);
  wifi_config.beacon_interval = 1000;
  wifi_softap_set_config(&wifi_config);

  if (Debug_Serial) {
    
    sprintf(text,"esp_read_mac():  %02x:%02x:%02x:%02x:%02x:%02x\r\n",
            WiFi_mac_addr[0],WiFi_mac_addr[1],WiFi_mac_addr[2],
            WiFi_mac_addr[3],WiFi_mac_addr[4],WiFi_mac_addr[5]);
    Serial.print(text);
  }

#endif // WIFI

  return;
}

/*
 *
 */

int transmit_wifi2(uint8_t *buffer,int length) {

#if ID_OD_WIFI

  if (length) {

    wifi_send_pkt_freedom(buffer,length,1);
  }

#endif

  return 0;
}

/*
 *
 */

int transmit_ble2(uint8_t *ble_message,int length) {

  return 0;
}

/*
 *
 */

#endif

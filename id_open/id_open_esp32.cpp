/* -*- tab-width: 2; mode: c; -*-
 * 
 * C++ class for Arduino to function as a wrapper around opendroneid.
 * This file has the ESP32 specific code.
 *
 * Copyright (c) 2020-2022, Steve Jack.
 *
 * Nov. '22:  Split out from id_open.cpp. 
 *
 * MIT licence.
 *
 * NOTES
 *
 * Bluetooth 4 works well with the opendroneid app on my G7.
 * WiFi beacon works with an ESP32 scanner, but with the G7 only the occasional frame gets through.
 *
 * Features
 *
 * esp_wifi_80211_tx() seems to zero the WiFi timestamp in addition to setting the sequence.
 * (The timestamp is set in ID_OpenDrone::transmit_wifi(), but WireShark says that it is zero.)
 *
 * BLE
 * 
 * A case of fighting the API to get it to do what I want.
 * For certain things, it is easier to bypass the 'user friendly' Arduino API and
 * use the esp_ functions.
 * 
 * Reference 
 * 
 * https://github.com/opendroneid/receiver-android/issues/7
 * 
 * From the Android app -
 * 
 * OpenDroneID Bluetooth beacons identify themselves by setting the GAP AD Type to
 * "Service Data - 16-bit UUID" and the value to 0xFFFA for ASTM International, ASTM Remote ID.
 * https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
 * https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos/
 * Vol 3, Part B, Section 2.5.1 of the Bluetooth 5.1 Core Specification
 * The AD Application Code is set to 0x0D = Open Drone ID.
 * 
    private static final UUID SERVICE_UUID = UUID.fromString("0000fffa-0000-1000-8000-00805f9b34fb");
    private static final byte[] OPEN_DRONE_ID_AD_CODE = new byte[]{(byte) 0x0D};
 * 
 */

#define DIAGNOSTICS 0

//

#if defined(ARDUINO_ARCH_ESP32)

#pragma GCC diagnostic warning "-Wunused-variable"

#include <Arduino.h>

#include "id_open.h"

#if ID_OD_WIFI 

#include <WiFi.h>

#include <esp_system.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,const void *buffer,int len,bool en_sys_seq);

static Stream              *Debug_Serial = NULL;

#endif // WIFI

#if ID_OD_BT

#include "BLEDevice.h"
#include "BLEUtils.h"

static esp_ble_adv_data_t   advData;
static esp_ble_adv_params_t advParams;
static BLEUUID              service_uuid;

#endif // BT

/*
 *
 */

void construct2() {

#if ID_OD_BT

  memset(&advData,0,sizeof(advData));

  advData.set_scan_rsp        = false;
  advData.include_name        = false;
  advData.include_txpower     = false;
  advData.min_interval        = 0x0006;
  advData.max_interval        = 0x0050;
  advData.flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  memset(&advParams,0,sizeof(advParams));

  advParams.adv_int_min       = 0x0020;
  advParams.adv_int_max       = 0x0040;
  advParams.adv_type          = ADV_TYPE_IND;
  advParams.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map       = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  advParams.peer_addr_type    = BLE_ADDR_TYPE_PUBLIC;

  service_uuid = BLEUUID("0000fffa-0000-1000-8000-00805f9b34fb");

#endif // ID_OD_BT

  return;
}

/*
 *
 */

void init2(char *ssid,int ssid_length,uint8_t *WiFi_mac_addr,uint8_t wifi_channel) {

  int  status;
  char text[128];

  status  = 0;
  text[0] = text[63] = 0;

#if DIAGNOSTICS
  Debug_Serial = &Serial;
#endif

#if ID_OD_WIFI

  int8_t                wifi_power;
  wifi_config_t         wifi_config;
	static wifi_country_t country = {"GB", 1, 13, 20, WIFI_COUNTRY_POLICY_AUTO};

  WiFi.softAP(ssid,"password",wifi_channel);

  esp_wifi_get_config(WIFI_IF_AP,&wifi_config);
  
  // wifi_config.ap.ssid_hidden = 1;
  status = esp_wifi_set_config(WIFI_IF_AP,&wifi_config);

  esp_wifi_set_country(&country);
  status = esp_wifi_set_bandwidth(WIFI_IF_AP,WIFI_BW_HT20);

  // esp_wifi_set_max_tx_power(78);
  esp_wifi_get_max_tx_power(&wifi_power);

  String address = WiFi.macAddress();

  status = esp_read_mac(WiFi_mac_addr,ESP_MAC_WIFI_STA);  

  if (Debug_Serial) {
    
    /*
    sprintf(text,"WiFi.macAddress: %s\r\n",address.c_str());
    Serial.print(text);
    */
    sprintf(text,"esp_read_mac():  %02x:%02x:%02x:%02x:%02x:%02x\r\n",
            WiFi_mac_addr[0],WiFi_mac_addr[1],WiFi_mac_addr[2],
            WiFi_mac_addr[3],WiFi_mac_addr[4],WiFi_mac_addr[5]);
    Serial.print(text);
// power <= 72, dbm = power/4, but 78 = 20dbm. 
    sprintf(text,"max_tx_power():  %d dBm\r\n",(int) ((wifi_power + 2) / 4));
    Debug_Serial->print(text);
  }

#endif // WIFI

#if ID_OD_BT

  int               power_db; 
  esp_power_level_t power;

  BLEDevice::init(ssid);

  // Using BLEDevice::setPower() seems to have no effect. 
  // ESP_PWR_LVL_N12 ...  ESP_PWR_LVL_N0 ... ESP_PWR_LVL_P9

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT,ESP_PWR_LVL_P9); 
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,ESP_PWR_LVL_P9); 

  power    = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
  power_db = 3 * ((int) power - 4); 

#endif

  return;
}

/*
 *
 */

uint8_t *capability() {

  // 0x21 = ESS | Short preamble
  // 0x04 = Short slot time

  static uint8_t capa[2] = {0x21,0x04};
  
  return capa;
}

/*
 *
 */

int transmit_wifi2(uint8_t *buffer,int length) {

  esp_err_t wifi_status = 0;

#if ID_OD_WIFI

  if (length) {

    wifi_status = esp_wifi_80211_tx(WIFI_IF_AP,buffer,length,true);  
  }

#endif

  return (int) wifi_status;
}

/*
 *
 */

int transmit_ble2(uint8_t *ble_message,int length) {

  esp_err_t  ble_status;
  static int advertising = 0; 

#if ID_OD_BT

  if (advertising) {

    ble_status = esp_ble_gap_stop_advertising();
  }

  ble_status = esp_ble_gap_config_adv_data_raw(ble_message,length); 
  ble_status = esp_ble_gap_start_advertising(&advParams);

  advertising = 1;

#endif // BT

  return (int) ble_status;
}

/*
 *
 */

#endif

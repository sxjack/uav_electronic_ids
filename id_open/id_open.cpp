/* -*- tab-width: 2; mode: c; -*-
 * 
 * C++ class for Arduino to function as a wrapper around opendroneid.
 *
 * Copyright (c) 2020, Steve Jack.
 *
 * MIT licence.
 *
 * Wifi
 * 
 * Needs testing against a known good app. 
 * 
 * BLE
 * 
 * A case of fighting the API to get it to do what I want.
 * For certain things, it is easier to bypass the 'user friendly' Arduino API and
 * use the esp functions.
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
     
#define DIAGNOSTICS 1

//

#if defined(ARDUINO_ARCH_ESP32)

#pragma GCC diagnostic warning "-Wunused-variable"

#include <Arduino.h>
#include <sys/time.h>

#include "id_open.h"

#if ID_OD_WIFI

#include <WiFi.h>

#include <esp_system.h>

extern "C" {
#include <esp_wifi.h>
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,const void *buffer,int len,bool en_sys_seq);
}

#endif

/*
 *
 */

ID_OpenDrone::ID_OpenDrone() {

  static const char *dummy = "";

  //

  UAS_operator = (char *) dummy;

#if ID_OD_WIFI

  wifi_channel = 6;

  memset(WiFi_mac_addr,0,6);
  memset(ssid,0,sizeof(ssid));
  
#endif

#if ID_OD_ASTM_BT | ID_OD_0_64_3_BT

  memset(&advData,0,sizeof(advData));

  advData.set_scan_rsp        = false;
  advData.include_name        = false;
  advData.include_txpower     = false;
  advData.min_interval        = 0x0006;
  advData.max_interval        = 0x0050;
  advData.flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  // advData.service_uuid_len    = 16;
  // advData.p_service_uuid      = (uint8_t *) &service_uuid;

  memset(&advParams,0,sizeof(advParams));

  advParams.adv_int_min       = 0x0020;
  advParams.adv_int_max       = 0x0040;
  advParams.adv_type          = ADV_TYPE_IND;
  advParams.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map       = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  advParams.peer_addr_type    = BLE_ADDR_TYPE_PUBLIC;

// 0000fffa-0000-1000-8000-00805f9b34fb

  memset(service_uuid,0,sizeof(service_uuid));

  service_uuid[13] = 0xff;
  service_uuid[12] = 0xfa;
  service_uuid[9]  = 0x10;
  service_uuid[7]  = 0x80;
  
  service_uuid[5]  = 0x00;
  service_uuid[4]  = 0x80;
  service_uuid[3]  = 0x5f;
  service_uuid[2]  = 0x9b;
  service_uuid[1]  = 0x34;
  service_uuid[0]  = 0xfb;

#endif // 0.64.3 | ASTM

  //

  memset(&UAS_data,0,sizeof(ODID_UAS_Data));

  basicID_data    = &UAS_data.BasicID;
  location_data   = &UAS_data.Location;
  auth_data       = &UAS_data.Auth[0];
  selfID_data     = &UAS_data.SelfID;
  system_data     = &UAS_data.System;
  operatorID_data = &UAS_data.OperatorID;

  basicID_data->IDType              = ODID_IDTYPE_NONE; // 0
  basicID_data->UAType              = ODID_UATYPE_NONE; // 0

  odid_initLocationData(location_data);

  location_data->Status             = ODID_STATUS_UNDECLARED; // 0
  location_data->SpeedVertical      = INV_SPEED_V;
  location_data->HeightType         = ODID_HEIGHT_REF_OVER_TAKEOFF;
  location_data->HorizAccuracy      = ODID_HOR_ACC_10_METER;
  location_data->VertAccuracy       = ODID_VER_ACC_10_METER;
  location_data->BaroAccuracy       = ODID_VER_ACC_10_METER;
  location_data->SpeedAccuracy      = ODID_SPEED_ACC_10_METERS_PER_SECOND;
  location_data->TSAccuracy         = ODID_TIME_ACC_1_0_SECOND;

  auth_data->AuthType               = ODID_AUTH_NONE; // 0

  selfID_data->DescType             = ODID_DESC_TYPE_TEXT;
  strcpy(selfID_data->Desc,"Model Aircraft");

  odid_initSystemData(system_data);

  system_data->OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
  system_data->ClassificationType   = ODID_CLASSIFICATION_TYPE_EU;
  system_data->AreaCount            = 1;
  system_data->CategoryEU           = ODID_CATEGORY_EU_SPECIFIC;
  system_data->ClassEU              = ODID_CLASS_EU_UNDECLARED;

  operatorID_data->OperatorIdType   = ODID_OPERATOR_ID;

  return;
}

/*
 *
 */

void ID_OpenDrone::init(char *op) {

  int  status;
  char text[128];

  status  = 0;
  text[0] = text[63] = 0;

#if DIAGNOSTICS
  Debug_Serial = &Serial;
#endif

  //

  UAS_operator = op;

  strncpy(operatorID_data->OperatorId,op,ODID_ID_SIZE);
  operatorID_data->OperatorId[sizeof(operatorID_data->OperatorId) - 1] = 0;

  basicID_data->IDType              = ODID_IDTYPE_CAA_REGISTRATION_ID;
  basicID_data->UAType              = ODID_UATYPE_NONE;
  strncpy(basicID_data->UASID,op,ODID_ID_SIZE);
  basicID_data->UASID[sizeof(basicID_data->UASID) - 1] = 0;

  //

  encodeBasicIDMessage(&basicID_enc,basicID_data);
  encodeLocationMessage(&location_enc,location_data);
  encodeAuthMessage(&auth_enc,auth_data);
  encodeSelfIDMessage(&selfID_enc,selfID_data);
  encodeSystemMessage(&system_enc,system_data);
  encodeOperatorIDMessage(&operatorID_enc,operatorID_data);

  //

#if ID_OD_WIFI

  int           i;
  wifi_config_t wifi_config;

  strncpy(ssid,op,i = sizeof(ssid)); ssid[i - 1] = 0;
  WiFi.softAP(ssid,NULL,wifi_channel);

  esp_wifi_get_config(WIFI_IF_AP,&wifi_config);
  
  wifi_config.ap.ssid_hidden = 1;
  
  status = esp_wifi_set_config(WIFI_IF_AP,&wifi_config);

  // esp_wifi_set_country();
  status = esp_wifi_set_bandwidth(WIFI_IF_AP,WIFI_BW_HT20);

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
  }

#endif

#if ID_OD_ASTM_BT | ID_OD_0_64_3_BT

  BLEDevice::init(UAS_operator);
  BLEDevice::setPower(ESP_PWR_LVL_P4); // ESP_PWR_LVL_N14 ...  ESP_PWR_LVL_N2 ... ESP_PWR_LVL_P4, ESP_PWR_LVL_P7

#endif

  return;
}

/*
 *
 */

int ID_OpenDrone::transmit(struct UTM_data *utm_data) {

  int                     status, valid_data;
  char                    text[128];
  uint32_t                msecs;
  static int              phase = 0;
  static uint32_t         last_msecs = 0;

  //

  text[0] = 0;
  msecs   = millis();

  // 

  if ((!system_data->OperatorLatitude)&&(utm_data->base_valid)) {

    system_data->OperatorLatitude  = utm_data->base_latitude;
    system_data->OperatorLongitude = utm_data->base_longitude;

    encodeSystemMessage(&system_enc,system_data);
  }

  //

  valid_data               =
  UAS_data.BasicIDValid    =
  UAS_data.LocationValid   =
  UAS_data.SelfIDValid     =
  UAS_data.SystemValid     =
  UAS_data.OperatorIDValid = 0;

  if ((msecs - last_msecs) > 74) {

    last_msecs = msecs;

    switch (++phase) {

    case  4: case  8: case 12: // Every 300 ms.
    case 16: case 20: case 24:
    case 28: case 32: case 36:

      if (utm_data->satellites >= SATS_LEVEL_2) {

        location_data->Direction       = (float) utm_data->heading;
        location_data->SpeedHorizontal = 0.514444 * (float) utm_data->speed_kn;
        location_data->SpeedVertical   = INV_SPEED_V;
        location_data->Latitude        = utm_data->latitude_d;
        location_data->Longitude       = utm_data->longitude_d;
        location_data->Height          = utm_data->alt_agl_m;
        location_data->AltitudeGeo     = utm_data->alt_msl_m;
    
        location_data->TimeStamp       = (float) ((utm_data->minutes * 60) + utm_data->seconds) +
                                         0.01 * (float) utm_data->csecs;

        if ((status = encodeLocationMessage(&location_enc,location_data)) == ODID_SUCCESS) {

          valid_data = UAS_data.LocationValid = 1;

          transmit_ble((uint8_t *) &location_enc,sizeof(location_enc));

        } else if (Debug_Serial) {

          sprintf(text,"ID_OpenDrone::%s, encodeLocationMessage returned %d\r\n",
                  __func__,status);
          Debug_Serial->print(text);
        }
      }

      break;

    case  6:

      valid_data = UAS_data.BasicIDValid    = 1;
      transmit_ble((uint8_t *) &basicID_enc,sizeof(basicID_enc));
      break;

    case 14:

      valid_data = UAS_data.SelfIDValid     = 1;
      transmit_ble((uint8_t *) &selfID_enc,sizeof(selfID_enc));
      break;

    case 22:

      valid_data = UAS_data.SystemValid     = 1;
      transmit_ble((uint8_t *) &system_enc,sizeof(system_enc));
      break;

    case 30:

      valid_data = UAS_data.OperatorIDValid = 1;
      transmit_ble((uint8_t *) &operatorID_enc,sizeof(operatorID_enc));
      break;

    default:

      if (phase > 39) {

        phase = 0;
      }

      break;
    }
  }

  //

  if (valid_data) {

#if ID_OD_WIFI
    status = transmit_wifi(utm_data);
#endif
  }

  return status;
}

/*
 *
 */

int ID_OpenDrone::transmit_wifi(struct UTM_data *utm_data) {

#if ID_OD_WIFI

  int                     length;
  char                    text[128];
  uint8_t                 buffer[1024];
  esp_err_t               wifi_status;
  static uint8_t          send_counter = 0;

  //

  length    = wifi_status = 0;
  buffer[0] = text[0]     = 0;

  //

  if ((length = odid_wifi_build_nan_sync_beacon_frame((char *) WiFi_mac_addr,
                                                      buffer,sizeof(buffer))) > 0) {

    wifi_status = esp_wifi_80211_tx(WIFI_IF_AP,buffer,length,true);  
  }
    
  if ((Debug_Serial)&&((length < 0)||(wifi_status != 0))) {

    sprintf(text,"odid_wifi_build_nan_sync_beacon_frame() = %d, esp_wifi_80211_tx() = %d\r\n",
            length,(int) wifi_status);
    Debug_Serial->print(text);
  }

  if ((length = odid_wifi_build_message_pack_nan_action_frame(&UAS_data,(char *) WiFi_mac_addr,
                                                              ++send_counter,
                                                              buffer,sizeof(buffer))) > 0) {

    wifi_status = esp_wifi_80211_tx(WIFI_IF_AP,buffer,length,true);
  }

  if ((Debug_Serial)&&((length < 0)||(wifi_status != 0))) {

    sprintf(text,"odid_wifi_build_message_pack_nan_action_frame() = %d, esp_wifi_80211_tx() = %d\r\n",
            length,(int) wifi_status);
    Debug_Serial->print(text);
  }
  
#endif

  return 0;
}

/*
 *
 */

int ID_OpenDrone::transmit_ble(uint8_t *odid_msg,int length) {

#if ID_OD_ASTM_BT | ID_OD_0_64_3_BT

  int         i, j, k, len;
  uint8_t    *a;
  esp_err_t   status;

  i = j = k = len = 0;
  a = ble_message;

  memset(ble_message,0,sizeof(ble_message));

  //
	
  if (advertising) {

    status = esp_ble_gap_stop_advertising();
  }

 
  ble_message[j++] = 0x1e;
#if ID_OD_ASTM_BT
  ble_message[j++] = 0x16; // 0xff
  ble_message[j++] = 0xfa; // ASTM
  ble_message[j++] = 0xff; //
#else
  ble_message[j++] = 0xff;
  ble_message[j++] = 0x02; // Intel
  ble_message[j++] = 0x00; //
#endif
  ble_message[j++] = 0x0d;

  ble_message[j++] = ++counter;

  for (i = 0; (i < length)&&(j < sizeof(ble_message)); ++i, ++j) {

    ble_message[j] = odid_msg[i];
  }

  status = esp_ble_gap_config_adv_data_raw(ble_message,len = j); 
  status = esp_ble_gap_start_advertising(&advParams);

  advertising = 1;

#if DIAGNOSTICS

  char       text[64], text2[34];
  static int first = 1;

  if (Debug_Serial) {

    if (first) {

      first = 0;

      Debug_Serial->print("0000000 00            ");

      for (i = 0; (i < 32); ++i) {

        sprintf(text,"%02d ",i);
        Debug_Serial->print(text);
      }

      Debug_Serial->print("\r\n");
    }

    sprintf(text,"%7lu %02x (%2d,%2d) .. ",
            millis(),len - 1,len - 1,length);
    Debug_Serial->print(text);

    for (i = 0; (i < len)&&(i < 32); ++i) {

      sprintf(text,"%02x ",a[i]);
      text2[i] = ((a[i] > 31)&&(a[i] < 127)) ? a[i]: '.';
      Debug_Serial->print(text);
    }

    text2[i] = 0;

    Debug_Serial->print(text2);
    Debug_Serial->print("\r\n");
  }

#endif

#endif // BT

  return 0;
}

/*
 *
 */

#endif

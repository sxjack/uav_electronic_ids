/* -*- tab-width: 2; mode: c; -*-
 *
 * C++ class for Arduino to function as a wrapper around opendroneid.
 *
 * Copyright (c) 2020-2021, Steve Jack.
 *
 * MIT licence.
 *
 */

#if defined(ARDUINO_ARCH_ESP32)

#ifndef ID_OPENDRONE_H
#define ID_OPENDRONE_H

/*
 *  Enabling both WiFi and Bluetooth will almost certainly require a partition scheme 
 *  with > 1.2M for the application.
 */

#define ID_OD_WIFI_NAN    0
#define ID_OD_WIFI_BEACON 1
#define ID_OD_ASTM_BT     1        // ASTM F3411-19.
#define ID_OD_0_64_3_BT   0        // Transmit a frame as defined in ODID specification version 0.64.3.

#define BLE_SERVICES      0        // Experimental.

#if ID_OD_WIFI_NAN || ID_OD_WIFI_BEACON
#define ID_OD_WIFI        1
#else
#define ID_OD_WIFI        0
#endif

#define BEACON_FRAME_SIZE 256

#if ID_OD_ASTM_BT || ID_OD_0_64_3_BT
#define ID_OD_BT          1
#else
#define ID_OD_BT          0
#endif

#define ID_OD_AUTH_DATUM  1546300800LU

//

#if ID_OD_BT
#include "BLEDevice.h"
#include "BLEUtils.h"
#endif

#include "utm.h"

#include "opendroneid.h"

//

class ID_OpenDrone {

public:
           ID_OpenDrone();
  void     init(struct UTM_parameters *);
  void     set_auth(char *);
  void     set_auth(uint8_t *,short int,uint8_t);
  int      transmit(struct UTM_data *);

private:

  int      transmit_wifi(struct UTM_data *);
  int      transmit_ble(uint8_t *,int);

  int                     auth_page = 0, auth_page_count = 0;
  char                   *UAS_operator;
  uint8_t                 msg_counter[16];
  int16_t                 phase = 0;
  Stream                 *Debug_Serial = NULL;

#if ID_OD_WIFI
  char                    ssid[32];
  uint8_t                 WiFi_mac_addr[6], wifi_channel;
#if ID_OD_WIFI_BEACON
  int                     beacon_offset = 0, beacon_max_packed = 30;
  uint8_t                 beacon_frame[BEACON_FRAME_SIZE],
                         *beacon_payload, *beacon_timestamp, *beacon_counter, *beacon_length; 
#endif
#endif

#if ID_OD_BT
  uint8_t                 ble_message[36], counter = 0;
  int                     advertising = 0;
  esp_ble_adv_data_t      advData;
  esp_ble_adv_params_t    advParams;
  BLEUUID                 service_uuid;
#if BLE_SERVICES
  BLEServer              *ble_server = NULL;
  BLEService             *ble_service_dbm = NULL;
  BLECharacteristic      *ble_char_dbm = NULL;
#endif
#endif

  ODID_UAS_Data           UAS_data;
  ODID_BasicID_data      *basicID_data;
  ODID_Location_data     *location_data;
  ODID_Auth_data         *auth_data[ODID_AUTH_MAX_PAGES];
  ODID_SelfID_data       *selfID_data;
  ODID_System_data       *system_data;
  ODID_OperatorID_data   *operatorID_data;

  ODID_BasicID_encoded    basicID_enc;
  ODID_Location_encoded   location_enc;
  ODID_Auth_encoded       auth_enc;
  ODID_SelfID_encoded     selfID_enc;
  ODID_System_encoded     system_enc;
  ODID_OperatorID_encoded operatorID_enc;
};

#endif

#endif // ESP32

/*
 *
 */

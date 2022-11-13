/* -*- tab-width: 2; mode: c; -*-
 * 
 * C++ class for Arduino to function as a wrapper around opendroneid.
 *
 * Copyright (c) 2020-2022, Steve Jack.
 *
 * Nov. '22:    Moved the processor specific code to a separate file.
 *
 * May '22:     opendroneid 2.0.
 *
 * Nov. '21:    Removed some redundant code. 
 *              Added option to use the new odid_wifi_build_message_pack_beacon_frame() function.
 * 
 * Oct. '21:    Updated for opendroneid release 1.0.
 *
 * May '21:     Packed WiFi.
 *
 * April '21:   Added support for beacon frames (untested). 
 *              Minor tidying up.
 *
 * January '21: Modified initialisation of BasicID.
 *              Authenication codes.
 * 
 *
 * MIT licence.
 *
 * NOTES
 *
 * 
 */
     
#define DIAGNOSTICS 0

//

#pragma GCC diagnostic warning "-Wunused-variable"

#include <Arduino.h>

#include <time.h>
#include <sys/time.h>

#include "id_open.h"

/*
 *
 */

ID_OpenDrone::ID_OpenDrone() {

  int                i;
  static const char *dummy = "";

  //

  UAS_operator = (char *) dummy;

#if ID_OD_WIFI

  memset(WiFi_mac_addr,0,6);
  memset(ssid,0,sizeof(ssid));

  strcpy(ssid,"UAS_ID_OPEN");

#if ID_OD_WIFI_BEACON

  // If ODID_PACK_MAX_MESSAGES == 10, then the potential size of the beacon message is > 255.
  
#if ODID_PACK_MAX_MESSAGES > 9
#undef ODID_PACK_MAX_MESSAGES
#define ODID_PACK_MAX_MESSAGES 9
#endif
  
  memset(beacon_frame,0,BEACON_FRAME_SIZE);

#if !USE_BEACON_FUNC

  beacon_counter   =
  beacon_length    =
  beacon_timestamp =
  beacon_payload   = beacon_frame;

#endif

#endif

#endif

  memset(msg_counter,0,sizeof(msg_counter));
  
  //
  // Below '// 0' indicates where we are setting 0 to 0 for clarity.
  //
  
  memset(&UAS_data,0,sizeof(ODID_UAS_Data));

  basicID_data    = &UAS_data.BasicID[0];
  location_data   = &UAS_data.Location;
  selfID_data     = &UAS_data.SelfID;
  system_data     = &UAS_data.System;
  operatorID_data = &UAS_data.OperatorID;

  for (i = 0; i < ODID_AUTH_MAX_PAGES; ++i) {

    auth_data[i] = &UAS_data.Auth[i];

    auth_data[i]->DataPage = i;
    auth_data[i]->AuthType = ODID_AUTH_NONE; // 0
  }

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

  selfID_data->DescType             = ODID_DESC_TYPE_TEXT;
  strcpy(selfID_data->Desc,"Recreational");

  odid_initSystemData(system_data);

  system_data->OperatorLocationType = ODID_OPERATOR_LOCATION_TYPE_TAKEOFF;
  system_data->ClassificationType   = ODID_CLASSIFICATION_TYPE_EU;
  system_data->AreaCount            = 1;
  system_data->AreaRadius           = 500;
  system_data->AreaCeiling          =
  system_data->AreaFloor            = -1000.0;
  system_data->CategoryEU           = ODID_CATEGORY_EU_SPECIFIC;
  system_data->ClassEU              = ODID_CLASS_EU_UNDECLARED;
  system_data->OperatorAltitudeGeo  = -1000.0;

  operatorID_data->OperatorIdType   = ODID_OPERATOR_ID;

  //

  construct2();
  
  return;
}

/*
 *
 */

void ID_OpenDrone::init(UTM_parameters *parameters) {

  int  status, i;
  char text[128];

  status  = 0;
  text[0] = text[63] = 0;

#if DIAGNOSTICS
  Debug_Serial = &Serial;
#endif

  // operator

  UAS_operator = parameters->UAS_operator;

  strncpy(operatorID_data->OperatorId,parameters->UAS_operator,ODID_ID_SIZE);
  operatorID_data->OperatorId[sizeof(operatorID_data->OperatorId) - 1] = 0;

  // basic

  basicID_data->UAType = (ODID_uatype_t) parameters->UA_type;
  basicID_data->IDType = (ODID_idtype_t) parameters->ID_type;

  switch(basicID_data->IDType) {

  case ODID_IDTYPE_SERIAL_NUMBER:

    strncpy(basicID_data->UASID,parameters->UAV_id,ODID_ID_SIZE);
    break;

  case ODID_IDTYPE_CAA_REGISTRATION_ID:

    strncpy(basicID_data->UASID,parameters->UAS_operator,ODID_ID_SIZE);
    break;    
  }
  
  basicID_data->UASID[sizeof(basicID_data->UASID) - 1] = 0;

  // system

  if (parameters->region < 2) {

    system_data->ClassificationType = (ODID_classification_type_t) parameters->region;
  }

  if (parameters->EU_category < 4) {

    system_data->CategoryEU = (ODID_category_EU_t) parameters->EU_category;
  }

  if (parameters->EU_class < 8) {

    system_data->ClassEU = (ODID_class_EU_t) parameters->EU_class;
  }

  //

  encodeBasicIDMessage(&basicID_enc,basicID_data);
  encodeLocationMessage(&location_enc,location_data);
  encodeAuthMessage(&auth_enc,auth_data[0]);
  encodeSelfIDMessage(&selfID_enc,selfID_data);
  encodeSystemMessage(&system_enc,system_data);
  encodeOperatorIDMessage(&operatorID_enc,operatorID_data);

  //

  if (UAS_operator[0]) {

    strncpy(ssid,UAS_operator,i = sizeof(ssid)); ssid[i - 1] = 0;
  }

  ssid_length = strlen(ssid);

  init2(ssid,ssid_length,WiFi_mac_addr,wifi_channel);

#if ID_OD_WIFI

#if ID_OD_WIFI_BEACON && !USE_BEACON_FUNC

  struct __attribute__((__packed__)) beacon_header {

    uint8_t control[2];          //  0-1:  frame control  
    uint8_t duration[2];         //  2-3:  duration
    uint8_t dest_addr[6];        //  4-9:  destination
    uint8_t src_addr[6];         // 10-15: source  
    uint8_t bssid[6];            // 16-21: base station
    uint8_t seq[2];              // 22-23: sequence
    uint8_t timestamp[8];        // 24-31: 
    uint8_t interval[2];         //
    uint8_t capability[2];
  } *header;

  header                  = (struct beacon_header *) beacon_frame;
  beacon_timestamp        = header->timestamp;

  header->control[0]      = 0x80;
  header->interval[0]     = 0xb8;
  header->interval[1]     = 0x0b;
  header->capability[0]   = 0x21; // ESS | Short preamble
  header->capability[1]   = 0x04; // Short slot time

  for (i = 0; i < 6; ++i) {

    header->dest_addr[i] = 0xff;
    header->src_addr[i]  = 
    header->bssid[i]     = WiFi_mac_addr[i];
  }
  
  beacon_offset = sizeof(struct beacon_header);

  beacon_frame[beacon_offset++] = 0;
  beacon_frame[beacon_offset++] = ssid_length;

  for (i = 0; (i < 32)&&(ssid[i]); ++i) {

    beacon_frame[beacon_offset++] = ssid[i];
  }

// Supported rates
#if 1
  beacon_frame[beacon_offset++] = 0x01; // This is what ODID 1.0 does.
  beacon_frame[beacon_offset++] = 0x01;
  beacon_frame[beacon_offset++] = 0x8c; // 11b, 6(B) Mbit/sec
#elif 0
  beacon_frame[beacon_offset++] = 0x01; // This is what the ESP32's beacon frames do. Jams GPS?
  beacon_frame[beacon_offset++] = 0x08;
  beacon_frame[beacon_offset++] = 0x8b; //  5.5
  beacon_frame[beacon_offset++] = 0x96; // 11
  beacon_frame[beacon_offset++] = 0x82; //  1
  beacon_frame[beacon_offset++] = 0x84; //  2
  beacon_frame[beacon_offset++] = 0x0c; //  6, note not 0x8c
  beacon_frame[beacon_offset++] = 0x18; // 12 
  beacon_frame[beacon_offset++] = 0x30; // 24
  beacon_frame[beacon_offset++] = 0x60; // 48
#endif

  // DS
  beacon_frame[beacon_offset++] = 0x03;
  beacon_frame[beacon_offset++] = 0x01;
  beacon_frame[beacon_offset++] = wifi_channel;
  
  // payload
  beacon_payload      = &beacon_frame[beacon_offset];
  beacon_offset      += 7;

  *beacon_payload++   = 0xdd;
  beacon_length       = beacon_payload++;

  *beacon_payload++   = 0xfa;
  *beacon_payload++   = 0x0b;
  *beacon_payload++   = 0xbc;

  *beacon_payload++   = 0x0d;
  beacon_counter      = beacon_payload++;

  beacon_max_packed   = BEACON_FRAME_SIZE - beacon_offset - 2;

  if (beacon_max_packed > (ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE)) {

    beacon_max_packed = (ODID_PACK_MAX_MESSAGES * ODID_MESSAGE_SIZE);
  }
  
#endif

#endif

  return;
}

/*
 *  These authentication functions need reviewing to make sure that they 
 *  comply with opendroneid release 1.0.
 */

void ID_OpenDrone::set_auth(char *auth) {

  set_auth((uint8_t *) auth,strlen(auth),0x0a);

  return;
}

//

void ID_OpenDrone::set_auth(uint8_t *auth,short int len,uint8_t type) {

  int      i, j;
  char     text[160];
  uint8_t  check[32];
  time_t   secs;

  auth_page_count = 1;

  time(&secs);

  if (len > MAX_AUTH_LENGTH) {

    len       = MAX_AUTH_LENGTH;
    auth[len] = 0;
  }
  
  auth_data[0]->AuthType = (ODID_authtype_t) type;

  for (i = 0; (i < 17)&&(auth[i]); ++i) {

    check[i]                  =
    auth_data[0]->AuthData[i] = auth[i];
  }
  
  check[i]                  = 
  auth_data[0]->AuthData[i] = 0;
  
  if (Debug_Serial) {

    sprintf(text,"Auth. Code \'%s\' (%d)\r\n",auth,len);
    Debug_Serial->print(text);

    sprintf(text,"Page 0 \'%s\'\r\n",check);
    Debug_Serial->print(text);
  }

  if (len > 16) {

    for (auth_page_count = 1; (auth_page_count < ODID_AUTH_MAX_PAGES)&&(i < len); ++auth_page_count) {

      auth_data[auth_page_count]->AuthType = (ODID_authtype_t) type;

      for (j = 0; (j < 23)&&(i < len); ++i, ++j) {

        check[j]                                = 
        auth_data[auth_page_count]->AuthData[j] = auth[i];
      }

      if (j < 23) {

        auth_data[auth_page_count]->AuthData[j] = 0;
      }
      
      check[j] = 0;
      
      if (Debug_Serial) {

        sprintf(text,"Page %d \'%s\'\r\n",auth_page_count,check);
        Debug_Serial->print(text);
      }
    }

    len = i;
  }

  auth_data[0]->LastPageIndex = (auth_page_count) ? auth_page_count - 1: 0;
  auth_data[0]->Length        = len;
  auth_data[0]->Timestamp     = (uint32_t) (secs - ID_OD_AUTH_DATUM);

  if (Debug_Serial) {

    sprintf(text,"%d pages\r\n",auth_page_count);
    Debug_Serial->print(text);
  }

  return;
}

/*
 *
 */

int ID_OpenDrone::transmit(struct UTM_data *utm_data) {

  int                     i, status,
                          valid_data, wifi_tx_flag_1, wifi_tx_flag_2;
  char                    text[128];
  uint32_t                msecs;
  time_t                  secs;
  static int              phase = 0;
  static uint32_t         last_msecs = 0;

  //

  text[0] = 0;
  msecs   = millis();

  // For the ODID 2.0 timestamp.
  // Does having a timestamp in system data mean that we should transmit it more often?
  time(&secs);

  // 

  if ((!system_data->OperatorLatitude)&&(utm_data->base_valid)) {

    system_data->OperatorLatitude    = utm_data->base_latitude;
    system_data->OperatorLongitude   = utm_data->base_longitude;
    system_data->OperatorAltitudeGeo = utm_data->base_alt_m;

    system_data->Timestamp           = (uint32_t) (secs - ID_OD_AUTH_DATUM);

    encodeSystemMessage(&system_enc,system_data);
  }

  //

  valid_data               =
  wifi_tx_flag_1           =
  wifi_tx_flag_2           = 0;

  UAS_data.BasicIDValid[0] =
  UAS_data.LocationValid   =
  UAS_data.SelfIDValid     =
  UAS_data.SystemValid     =
  UAS_data.OperatorIDValid = 0;

  for (i = 0; i < ODID_AUTH_MAX_PAGES; ++i) {

    UAS_data.AuthValid[i] = 0;
  }

  if ((msecs - last_msecs) > 74) {

    last_msecs = (last_msecs) ? last_msecs + 75: msecs;

    switch (++phase) {

    case  4: case  8: case 12: // Every 300 ms.
    case 16: case 20: case 24:
    case 28: case 32: case 36:

      wifi_tx_flag_1 = 1;

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

      if (basicID_data->IDType) {
        
        valid_data = UAS_data.BasicIDValid[0] = 1;
        transmit_ble((uint8_t *) &basicID_enc,sizeof(basicID_enc));
      }
      
      break;

    case 14:

      wifi_tx_flag_2 = valid_data =
      UAS_data.SelfIDValid        = 1;
      transmit_ble((uint8_t *) &selfID_enc,sizeof(selfID_enc));
      break;

    case 22:

      valid_data = UAS_data.SystemValid = 1;
#if 1
      if (secs > ID_OD_AUTH_DATUM) {

        system_data->Timestamp = (uint32_t) (secs - ID_OD_AUTH_DATUM);
        encodeSystemMessage(&system_enc,system_data);
      }
#endif
      transmit_ble((uint8_t *) &system_enc,sizeof(system_enc));
      break;

    case 30:

      valid_data = UAS_data.OperatorIDValid = 1;
      transmit_ble((uint8_t *) &operatorID_enc,sizeof(operatorID_enc));
      break;

    case 38:

      if (auth_page_count) {

        encodeAuthMessage(&auth_enc,auth_data[auth_page]);
        valid_data = UAS_data.AuthValid[auth_page] = 1;

        transmit_ble((uint8_t *) &auth_enc,sizeof(auth_enc));

        if (++auth_page >= auth_page_count) {

          auth_page = 0;
        }
      }

      break;

    default:

      if (phase > 39) {

        phase = 0;
      }

      break;
    }
  }

  //

#if ID_OD_WIFI

#if 0

  // Don't pack the WiFi data, send one message at a time.

  if (valid_data) {

    status = transmit_wifi(utm_data);
  }

#else

  // Pack the WiFi data.
  // One group every 300ms and another every 3000ms.
  
  if (wifi_tx_flag_1) { // IDs and locations.

    UAS_data.SystemValid = 1;

    if (UAS_data.BasicID[0].UASID[0]) {

      UAS_data.BasicIDValid[0] = 1;
    }

    if (UAS_data.OperatorID.OperatorId[0]) {

      UAS_data.OperatorIDValid = 1;
    }

    status = transmit_wifi(utm_data);

  } else if (wifi_tx_flag_2) { // SelfID and authentication.

    for (i = 0; (i < auth_page_count)&&(i < ODID_AUTH_MAX_PAGES); ++i) {

      UAS_data.AuthValid[i] = 1;
    }
      
    status = transmit_wifi(utm_data);
  }

#endif // Pack data

#endif // ID_OD_WIFI

return status;
}

/*
 *
 */

int ID_OpenDrone::transmit_wifi(struct UTM_data *utm_data) {

#if ID_OD_WIFI

  int length, wifi_status;

#if ID_OD_WIFI_NAN

  char           text[128];
  uint8_t        buffer[1024];
  static uint8_t send_counter = 0;

  if ((length = odid_wifi_build_nan_sync_beacon_frame((char *) WiFi_mac_addr,
                                                      buffer,sizeof(buffer))) > 0) {

    wifi_status = transmit_wifi2(buffer,length);
  }
    
  if ((Debug_Serial)&&((length < 0)||(wifi_status != 0))) {

    sprintf(text,"odid_wifi_build_nan_sync_beacon_frame() = %d, transmit_wifi2() = %d\r\n",
            length,(int) wifi_status);
    Debug_Serial->print(text);
  }

  if ((length = odid_wifi_build_message_pack_nan_action_frame(&UAS_data,(char *) WiFi_mac_addr,
                                                              ++send_counter,
                                                              buffer,sizeof(buffer))) > 0) {

    wifi_status = transmit_wifi2(buffer,length);
  }

  if ((Debug_Serial)&&((length < 0)||(wifi_status != 0))) {

    sprintf(text,"odid_wifi_build_message_pack_nan_action_frame() = %d, transmit_wifi2() = %d\r\n",
            length,(int) wifi_status);
    Debug_Serial->print(text);
  }

#endif // NAN
  
#if ID_OD_WIFI_BEACON

#if USE_BEACON_FUNC

  if ((length = odid_wifi_build_message_pack_beacon_frame(&UAS_data,(char *) WiFi_mac_addr,
                                                          ssid,ssid_length,
                                                          3000,++beacon_counter,
                                                          beacon_frame,BEACON_FRAME_SIZE)) > 0) {

    wifi_status = transmit_wifi2(beacon_frame,length);
  }

#if DIAGNOSTICS && 1

  if (Debug_Serial) {

    char text[128];

    sprintf(text,"ID_OpenDrone::%s * %02x ... ",__func__,beacon_frame[0]);
    Debug_Serial->print(text);

    for (int i = 0; i < 20; ++i) {

      sprintf(text,"%02x ",beacon_frame[22 + i]);
      Debug_Serial->print(text);      
    }

    Debug_Serial->print(" ... *\r\n");
  }

#endif // DIAG

#else
  
  int      i, len2 = 0;
  uint64_t usecs;

  ++*beacon_counter;

  usecs = micros();

  for (i = 0; i < 8; ++i) {

    beacon_timestamp[i] = (usecs >> (i * 8)) & 0xff;
  }

  if ((length = odid_message_build_pack(&UAS_data,beacon_payload,beacon_max_packed)) > 0) {

    *beacon_length = length + 5;
    
    wifi_status = transmit_wifi2(beacon_frame,len2 = beacon_offset + length);
  }

#if DIAGNOSTICS && 1

  char text[128];

  if (Debug_Serial) {

    sprintf(text,"ID_OpenDrone::%s %d %d+%d=%d ",
            __func__,beacon_max_packed,beacon_offset,length,len2);
    Debug_Serial->print(text);

    sprintf(text,"* %02x ... ",beacon_frame[0]);
    Debug_Serial->print(text);

    for (int i = 0; i < 16; ++i) {

      if ((i == 3)||(i == 10)) {

        Debug_Serial->print("| ");
      }

      sprintf(text,"%02x ",beacon_frame[beacon_offset - 10 + i]);
      Debug_Serial->print(text);
    }

    sprintf(text,"... %02x\r\n",beacon_frame[len2 - 1]);
    Debug_Serial->print(text);
  }

#endif // DIAG

#endif // FUNC
  
#endif // BEACON

#endif // WIFI

  return 0;
}

/*
 *
 */

int ID_OpenDrone::transmit_ble(uint8_t *odid_msg,int length) {

#if ID_OD_BT

  int         i, j, k, len, status;
  uint8_t    *a;

  i = j = k = len = 0;
  a = ble_message;

  memset(ble_message,0,sizeof(ble_message));

  //

  ble_message[j++] = 0x1e;
  ble_message[j++] = 0x16;
  ble_message[j++] = 0xfa; // ASTM
  ble_message[j++] = 0xff; //
  ble_message[j++] = 0x0d;

#if 0
  ble_message[j++] = ++counter;
#else
  ble_message[j++] = ++msg_counter[odid_msg[0] >> 4];
#endif

  for (i = 0; (i < length)&&(j < sizeof(ble_message)); ++i, ++j) {

    ble_message[j] = odid_msg[i];
  }

  status = transmit_ble2(ble_message,len = j); 

#if DIAGNOSTICS && 0

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

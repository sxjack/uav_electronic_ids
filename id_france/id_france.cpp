/* -*- tab-width: 2; mode: c; -*-
 * 
 * C++ class to implment a French UAS eID.
 *
 * Copyright (c) 2020, Steve Jack.
 *
 * MIT licence.
 *
 * 21/01/xx Modified so that it will transmit operator ID or serial number.
 *          Floats changed to doubles.
 *          Calculation of m/deg moved to common support library.
 *
 *
 * This class was inspired by droneID_FR.h by Pierre Kancir (https://github.com/khancyr/droneID_FR). 
 *
 * Reference
 *
 * joe_20191229_0302_0039.pdf
 *
 * Status
 *
 * Needs testing against a known good app.
 * There is an offset of 4 bytes (the OUI ?) when tested against the Gendarmerie Nationale python script.
 * Works with 'recepteur_balise.ino' (http://icnisnlycee.free.fr/index.php/57-nsi/projets/75-balise-de-signalement-pour-aeronefs-sans-personne-a-bord)
 * 
 */

#define DIAGNOSTICS 1

#define SATS_REQ    SATS_LEVEL_2

//

#if defined(ARDUINO_ARCH_ESP32)

#pragma GCC diagnostic warning "-Wunused-variable"

#include <Arduino.h>

#include <time.h>
#include <sys/time.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

#include <esp_system.h>

extern "C" {
#include <esp_wifi.h>
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,const void *buffer,int len,bool en_sys_seq);
}

#include "id_france.h"

/*
 *
 */

ID_France::ID_France() {

  int                i;
  static const char *dummy = "";

  UAS_operator = (char *) dummy;
  UAV_id       = (char *) dummy;

  wifi_channel = 6; // Do not change.

  memset(wifi_mac_addr,0,6);
  memset(ssid,0,sizeof(ssid));
  memset(manufacturer,'0',i = sizeof(manufacturer)); manufacturer[i - 1] = 0;
  strncpy(model,"000",i = sizeof(model));            model[i - 1]        = 0;
  memset(wifi_frame,0,sizeof(wifi_frame));

  header = (struct fid_header *) wifi_frame;

  return;
}

/*
 *
 */

void ID_France::init(const char *op) {

  init(op,NULL);
  
  return;
}

//

void ID_France::init(struct UTM_parameters *parameters) {

  init(parameters->UAS_operator,parameters->UAV_id);
  
  return;
}

//

void ID_France::init(const char *op,const char *uav) {

  int            i, offset;
  char           text[128];
  double         lat_d, long_d;
  int8_t         max_power = 0;
  const uint32_t wifi_oui = ID_FRANCE_OUI;
  wifi_config_t  wifi_config;


  text[0]  = 
  text[63] = 0;

#if DIAGNOSTICS
  Debug_Serial = &Serial;
#endif

  //

  UAS_operator = (char *) op;
  
  strncpy(ssid,op,i = sizeof(ssid));

  ssid[(i > 0) ? i - 1: 0] = 0;

  if (uav) {

    UAV_id = (char *) uav;
  }
  
  //

  WiFi.softAP(ssid,NULL,wifi_channel);

  esp_wifi_get_config(WIFI_IF_AP,&wifi_config);
  
  wifi_config.ap.beacon_interval = 1000;
  wifi_config.ap.ssid_hidden     = 1;
  
  esp_wifi_set_config(WIFI_IF_AP,&wifi_config);

  // esp_wifi_set_country();
  esp_wifi_set_bandwidth(WIFI_IF_AP,WIFI_BW_HT20);

  // esp_wifi_set_max_tx_power(78);
  esp_wifi_get_max_tx_power(&max_power);
  
  //

  header->control[0]        = 0x80;
  header->interval[0]       = 0xb8;
  header->interval[1]       = 0x0b;
  header->capability[0]     = 0x21;
  header->capability[1]     = 0x04;
  header->ds_parameter[0]   = 0x03;
  header->ds_parameter[1]   = 0x01;
  header->ds_parameter[2]   = wifi_channel;

  String address = WiFi.macAddress();

  esp_read_mac(wifi_mac_addr,ESP_MAC_WIFI_STA);

  for (i = 0; i < 6; ++i) {

    header->dest_addr[i] = 0xff;
    header->src_addr[i]  = 
    header->bssid[i]     = wifi_mac_addr[i];
  }

  if (Debug_Serial) {

    // sprintf(text,"WiFi.macAddress: %s\r\n",address.c_str());
    // Debug_Serial->print(text);
    sprintf(text,"esp_read_mac():  %02x:%02x:%02x:%02x:%02x:%02x\r\n",
            wifi_mac_addr[0],wifi_mac_addr[1],wifi_mac_addr[2],
            wifi_mac_addr[3],wifi_mac_addr[4],wifi_mac_addr[5]);
    Debug_Serial->print(text);

    sprintf(text,"max_tx_power():  %d dBm\r\n",(int) ((max_power + 2) / 4));
    Debug_Serial->print(text);
  }

  offset = sizeof(fid_header);

  wifi_frame[offset++] = 0;
  wifi_frame[offset++] = strlen(ssid);

  for (i = 0; (i < 32)&&(ssid[i]); ++i) {

    wifi_frame[offset++] = ssid[i];
  }

  frame_length = offset + sizeof(struct fid_payload);
  payload      = (struct fid_payload *) &wifi_frame[offset]; 

  payload->preamble[0] = 0xdd;
  payload->preamble[1] = sizeof(struct fid_payload) - 2; // ?

  payload->preamble[2] = (wifi_oui >> 16) & 0xff;
  payload->preamble[3] = (wifi_oui >>  8) & 0xff;
  payload->preamble[4] =  wifi_oui        & 0xff;
  payload->preamble[5] = 0x01; // Vendor specific type.

  payload->T1      = 0x01;
  payload->L1      = sizeof(payload->version);
  payload->version = 0x01;

  payload->L2_3    = sizeof(payload->id_france);

  if (*UAV_id) {

    payload->T2_3 = 0x03;

    strncpy((char *) payload->id_france,UAV_id,i = sizeof(payload->id_france)); 
    payload->id_france[i - 1] = 0;
    
  } else {

    payload->T2_3 = 0x02;

    for (i = 0; i < 3; ++i) {

      payload->id_france[i]   = manufacturer[i];
      payload->id_france[i+3] = model[i];
    }

    strncpy((char *) &payload->id_france[6],UAS_operator,(i = sizeof(payload->id_france)) - 6); 
    payload->id_france[i - 1] = 0;
  }

  payload->T4      =  4;
  payload->L4      = sizeof(payload->latitude);
  payload->T5      =  5;
  payload->L5      = sizeof(payload->longitude);

#if DIAGNOSTICS && 0

  // An example from the specification.

  lat_d  =   48.15278;
  long_d = -179.12345;

  encode_latlong(lat_d,long_d,payload->latitude,payload->longitude);

  if (Debug_Serial) {

    char lat_s[16], long_s[16], text2[16];

    dtostrf(lat_d,10,5,lat_s);
    dtostrf(long_d,10,5,long_s);

    Debug_Serial->print("Example lat./long. encoding\r\n");

    sprintf(text,"%s -> %02x %02x %02x %02x\r\n",lat_s,
            payload->latitude[0],payload->latitude[1],
            payload->latitude[2],payload->latitude[3]);
    Debug_Serial->print(text);

    sprintf(text,"%s -> %02x %02x %02x %02x\r\n",long_s,
            payload->longitude[0],payload->longitude[1],
            payload->longitude[2],payload->longitude[3]);
    Debug_Serial->print(text);
  }

#endif

  // DGAC HQ, 48 50'2.57" N 2 16'17.32" E

  lat_d  = 48.0 + (50.0 / 60.0) + ( 2.57 / 3600.0);
  long_d =  2.0 + (16.0 / 60.0) + (17.32 / 3600.0);

  encode_latlong(lat_d,long_d,payload->latitude,payload->longitude);
  encode_latlong(lat_d,long_d,payload->base_lat,payload->base_long);

  payload->T6          =  6;
  payload->L6          = sizeof(payload->altitude);
  payload->altitude[1] = 31; // m

  payload->T7          =  7;
  payload->L7          = sizeof(payload->height);

  payload->T8          =  8;
  payload->L8          = sizeof(payload->base_lat);

  payload->T9          =  9;
  payload->L9          = sizeof(payload->base_long);

  payload->T10         = 10;
  payload->L10         = sizeof(payload->ground_speed);

  payload->T11         = 11;
  payload->L11         = sizeof(payload->heading);

  //

  return;
}

/*
 *
 */

int ID_France::transmit(struct UTM_data *utm_data) {

  int                     i, length;
  char                    text[128];
  double                  lat_d, long_d, a, b, movement = 0.0;
  uint16_t                elapsed;
  uint32_t                msecs;
  uint64_t                usecs;
  esp_err_t               wifi_status;
  union {int16_t i16; uint16_t u16;} 
                          conv;

  length      = sizeof(wifi_frame);
  wifi_status = 0;
  text[0]     = 0;

  //

  if ((!m_deg_lat)&&(utm_data->base_valid)) {

    char lat_s[16], long_s[16];

    lat_d      = utm_data->base_latitude;
    long_d     = utm_data->base_longitude;

    encode_latlong(lat_d,long_d,payload->base_lat,payload->base_long);

    dtostrf(lat_d,11,6,lat_s);
    dtostrf(long_d,11,6,long_s);

    utm_utils.calc_m_per_deg(lat_d,&m_deg_lat,&m_deg_long);

    if (Debug_Serial) {

      sprintf(text,"Base %s,%s\r\n",lat_s,long_s);
      Debug_Serial->print(text);
      sprintf(text,"Lat.  %6d m/deg\r\n",(int) m_deg_lat);
      Debug_Serial->print(text);
      sprintf(text,"Long. %6d m/deg\r\n",(int) m_deg_long);
      Debug_Serial->print(text);
    }

    last_lat  = utm_data->base_latitude;
    last_long = utm_data->base_longitude;
  }

  lat_d      = utm_data->latitude_d;
  long_d     = utm_data->longitude_d;

  if (m_deg_lat) {

    a = (lat_d  - last_lat) * m_deg_lat;
    b = (long_d - last_long) * m_deg_long; 

    movement = sqrt((a*a) + (b*b));
  }

  //

  msecs   = millis();
  elapsed = msecs - last_locn_msecs;

  if ((((movement > 29.0)&&(elapsed > 250))||(elapsed > 2999))&&
      (utm_data->satellites >= SATS_REQ)) {

#if DIAGNOSTICS && 0
    if (Debug_Serial) {

      sprintf(text,"%d sats, HDOP %s, %d m, %u msecs\r\n",
              utm_data->satellites,utm_data->hdop_s,(int) movement,elapsed);
      Debug_Serial->print(text);
    }
#endif

    last_locn_msecs = msecs;

    ++sequence;

    header->seq[0] =  sequence       & 0xff;
    header->seq[1] = (sequence >> 8) & 0xff;

    usecs = micros();

    for (i = 0; i < 8; ++i) {

      header->timestamp[i] = (uint8_t) usecs;
      usecs >>= 8;
    }

    encode_latlong(utm_data->latitude_d,utm_data->longitude_d,
                   payload->latitude,payload->longitude);

    conv.i16              = (int16_t) (utm_data->alt_msl_m);
    payload->altitude[1]  =  conv.u16 & 0xff; 
    payload->altitude[0]  = (conv.u16 >> 8) & 0xff; 

    conv.i16              = (int16_t) (utm_data->alt_agl_m);
    payload->height[1]    =  conv.u16 & 0xff; 
    payload->height[0]    = (conv.u16 >> 8) & 0xff; 

    payload->ground_speed = (uint8_t) (utm_data->speed_kn * 0.514444);

    conv.i16             = (int16_t)  utm_data->heading;
    payload->heading[1]  =  conv.u16 & 0xff; 
    payload->heading[0]  = (conv.u16 >> 8) & 0xff; 

    wifi_status = esp_wifi_80211_tx(WIFI_IF_AP,wifi_frame,frame_length,true);

#if DIAGNOSTICS && 0
    dump_frame();
#endif
  }

  return wifi_status;
}

/*
 * 
 */

void ID_France::encode_latlong(double lat_d,double long_d,
                               uint8_t *lat_u8,uint8_t *long_u8) {

  int     i;
  union {int32_t i32; uint32_t u32;} 
          latitude, longitude;

  latitude.i32  = (int32_t) ( lat_d * 1.0e5);
  longitude.i32 = (int32_t) (long_d * 1.0e5);

  for (i = 0; i < 4; ++i) {

    lat_u8[3 - i]  = (uint8_t) (latitude.u32  & 0xff); 
    long_u8[3 - i] = (uint8_t) (longitude.u32 & 0xff); 

    latitude.u32  >>= 8;
    longitude.u32 >>= 8;
  }

  return;
}

/*
 *
 */

void ID_France::dump_frame() {

  int      i;
  char     text[128], text2[20];
  uint8_t *u8;

  text[0]     = 0;
  text2[0]    =
  text2[16]   = 0; 

  if (Debug_Serial) {

    u8 = (uint8_t *) &wifi_frame;

    sprintf(text,"\r\nFrame, %d bytes\r\n   ",frame_length);
    Debug_Serial->print(text);

    for (i = 0; i < 16; ++i) {

      sprintf(text,"%02d ",i);
      Debug_Serial->print(text);
    }

    Debug_Serial->print("\r\n 0 ");

    for (i = 0; i < (frame_length + 4);) {

      sprintf(text,"%02x ",u8[i]);
      Debug_Serial->print(text);

      text2[i % 16] = ((u8[i] > 31)&&(u8[i] < 127)) ? u8[i]: '.';

      if ((++i % 16) == 0) {

        sprintf(text,"%s\r\n%2d ",text2,i / 16);
        Debug_Serial->print(text);          
      }

      text2[i % 16] = 0;
    }
    
    Debug_Serial->print("\r\n\r\n");          
  }

  return;
}


/*
 *
 */

#endif // ESP32

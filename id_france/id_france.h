/* -*- tab-width: 2; mode: c; -*-
 *
 * C++ class to implment a French UAS eID.
 *
 * Copyright (c) 2020, Steve Jack.
 *
 * MIT licence.
 *
 */

#if defined(ARDUINO_ARCH_ESP32)

#ifndef ID_FRANCE_H
#define ID_FRANCE_H

#define ID_FRANCE_OUI 0x6a5c35

#include "utm.h"

struct __attribute__((__packed__)) fid_header {

  uint8_t control[2];          //  0-1:  frame control  
  uint8_t duration[2];         //  2-3:  duration
  uint8_t dest_addr[6];        //  4-9:  destination
  uint8_t src_addr[6];         // 10-15: source  
  uint8_t bssid[6];            // 16-21: base station
  uint8_t seq[2];              // 22-23: sequence
  uint8_t timestamp[8];        // 24-31: 
  uint8_t interval[2];         //
  uint8_t capability[2];
  uint8_t ds_parameter[3];     // : ds parameters
};

struct __attribute__((__packed__)) fid_payload {

  uint8_t preamble[6];
  uint8_t T1, L1;
  uint8_t version;
#if 1
  uint8_t T2_3, L2_3;
  uint8_t id_france[30];
#else
  uint8_t T2, L2;
  uint8_t id_france[30];
  uint8_t T3, L3;
  uint8_t id_ansi[30];
#endif
  uint8_t T4, L4;
  uint8_t latitude[4];
  uint8_t T5, L5;
  uint8_t longitude[4];
  uint8_t T6, L6;
  uint8_t altitude[2]; // m msl
  uint8_t T7, L7;
  uint8_t height[2]; // m agl
  uint8_t T8, L8;
  uint8_t base_lat[4];
  uint8_t T9, L9;
  uint8_t base_long[4];
  uint8_t T10, L10;
  uint8_t ground_speed; // m/s
  uint8_t T11, L11;
  uint8_t heading[2];
};

//

class ID_France {

 public:
            ID_France();
   void     init(struct UTM_parameters *);
   void     init(const char *);
   int      transmit(struct UTM_data *);

 private:

   void                 init(const char *,const char *);
   void                 encode_latlong(double,double,uint8_t *,uint8_t *);
   void                 dump_frame();

   int                  frame_length;
   char                *UAS_operator, *UAV_id, ssid[32],
                        manufacturer[4], model[4];
   double               last_lat, last_long, m_deg_lat = 0.0, m_deg_long = 0.0;
   int16_t              phase = 0, sequence = 0;
   uint8_t              wifi_channel, 
                        wifi_mac_addr[6], wifi_frame[256];
   uint32_t             last_locn_msecs = 0;
   Stream              *Debug_Serial = NULL;
   struct fid_header   *header = NULL;
   struct fid_payload  *payload = NULL;
   UTM_Utilities        utm_utils;
};

#endif

#endif

/*
 *
 */

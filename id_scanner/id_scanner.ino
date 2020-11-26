/* -*- tab-width: 2; mode: c; -*-
 * 
 * Scanner for WiFi direct remote id. 
 * Handles both opendroneid and French formats.
 * 
 * Copyright (c) 2020, Steve Jack.
 *
 * MIT licence.
 * 
 */

#if not defined(ARDUINO_ARCH_ESP32)
#error "This program requires an ESP32"
#endif

#pragma GCC diagnostic warning "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

//

#include <Arduino.h>

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>

#include "opendroneid.h"

//

#define DIAGNOSTICS        1

#define LCD_DISPLAY       11
#define DISPLAY_PAGE_MS 4000

#define TFT_DISPLAY        1
#define TFT_WIDTH        128
#define TFT_HEIGHT       160
#define TRACK_SCALE      1.0 // m/pixel
#define TRACK_TIME       120 // secs, 600

#define ID_SIZE     (ODID_ID_SIZE + 1)
#define MAX_UAVS           8

//

struct id_data {int      flag;
                uint8_t  mac[6];
                uint32_t last_seen;
                char     op_id[ID_SIZE];
                double   lat_d, long_d, base_lat_d, base_long_d;
                int      altitude_msl, height_agl, speed, heading, rssi;
};

//

static void      print_json(int,int,struct id_data *);
static esp_err_t event_handler(void *,system_event_t *);
static void      callback(void *,wifi_promiscuous_pkt_type_t);
static void      parse_french_id(struct id_data *,uint8_t *);
static void      dump_frame(uint8_t *,int);
static void      calc_m_per_deg(double,double,double *,double *);
  
static double           base_lat_d = 0.0, base_long_d = 0.0, m_deg_lat = 110000.0, m_deg_long = 110000.0;

volatile char           ssid[10];
volatile unsigned int   callback_counter = 0, french_wifi = 0, odid_wifi = 0;
volatile struct id_data uavs[MAX_UAVS + 1];

volatile ODID_UAS_Data  UAS_data;

//

#if LCD_DISPLAY 

static const char     *title = "ID Scanner", *build_date = __DATE__;

#endif

#if (LCD_DISPLAY > 10) && (LCD_DISPLAY < 20) 

#include <Wire.h>

#include <U8x8lib.h>

const int display_address = 0x78;

#if LCD_DISPLAY == 11
U8X8_SH1106_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
#elif LCD_DISPLAY == 12
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE);
#endif

#endif

#if TFT_DISPLAY

// For this library, the chip and pins are defined in User_Setup.h
// which is pretty horrible.

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
                  
static uint16_t *pixel_timestamp = NULL;
static uint32_t  track_colours[MAX_UAVS + 1];

#endif

/*
 * 
 */

void setup() {

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  //

  memset((void *) &UAS_data,0,sizeof(ODID_UAS_Data));
  memset((void *) uavs,0,(MAX_UAVS + 1) * sizeof(struct id_data));
  memset((void *) ssid,0,10);

  strcpy((char *) uavs[MAX_UAVS].op_id,"NONE");

  //

  delay(100);

  Serial.begin(115200);

  //

  nvs_flash_init();
  tcpip_adapter_init();

  esp_event_loop_init(event_handler,NULL);

  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&callback);

  // The channel should be 6.
  // If the second parameter is not WIFI_SECOND_CHAN_NONE, cast it to (wifi_second_chan_t).
  esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE);

  //

#if LCD_DISPLAY > 10

#if LCD_DISPLAY < 20
  u8x8.setI2CAddress(display_address);
#endif

  u8x8.begin();
  u8x8.setPowerSave(0);

  u8x8.setFont(u8x8_font_chroma48medium8_r);
 
  u8x8.refreshDisplay();

  u8x8.drawString( 3,0,(char *) title);
  u8x8.drawString( 3,1,(char *) build_date);
  u8x8.drawString( 1,2,"lat.");
  u8x8.drawString(13,2,"msl");
  u8x8.drawString( 1,3,"long.");
  u8x8.drawString(13,3,"agl");
  u8x8.drawString( 1,4,"base lat.");
  u8x8.drawString(13,4,"m/s");
  u8x8.drawString( 1,5,"base long.");
  u8x8.drawString(13,5,"deg");
  u8x8.drawString( 0,6,"ODID    heap");
  u8x8.drawString( 0,7,"French  stack");

#endif

#if TFT_DISPLAY

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  if ((pixel_timestamp = (uint16_t *) calloc(TFT_WIDTH * TFT_HEIGHT,sizeof(uint16_t))) == NULL) {

    Serial.print("{ \"message\": \"Unable to allocate memory for track data.\" }\r\n");
  }

#if MAX_UAVS != 8
#warning "Align MAX_UAVS and colour assignments."
#endif

  track_colours[0] = TFT_RED;
  track_colours[1] = TFT_GREEN;
  track_colours[2] = TFT_YELLOW;
  track_colours[3] = TFT_ORANGE;
  track_colours[4] = TFT_CYAN;
  track_colours[5] = TFT_BLUE;
  track_colours[6] = TFT_SILVER;
  track_colours[7] = TFT_PINK;
  
  track_colours[MAX_UAVS] = TFT_WHITE;

  for (int i = 0, y = 4; i < MAX_UAVS; ++i, y += 4) {

    tft.drawLine(4,y,10,y,track_colours[i]);
  }

#endif

  Serial.print("{ \"message\": \"setup() complete\" }\r\n");

  return;
}

/*
 *
 */

void loop() {

  int             i, j;
  char            text[128];
  double          x_m = 0.0, y_m = 0.0;
  uint32_t        msecs, secs;
  static int      display_uav = 0;
  static uint32_t last_display_update = 0, last_page_change = 0, last_json = 0;
#if LCD_DISPLAY
  char            text1[16];
  static int      display_phase = 0;
#endif
#if TFT_DISPLAY 
  int             x, y, index = 0;
  static int      clear_y = 0;
#endif

  text[0] = 0;

  //
  
  msecs = millis();
  secs  = msecs / 1000;

  //

  for (i = 0; i < MAX_UAVS; ++i) {

    if ((uavs[i].last_seen)&&
        ((msecs - uavs[i].last_seen) > 300000L)) {

      uavs[i].last_seen = 0;
      uavs[i].mac[0]    = 0;
    }

    if (uavs[i].flag) {

      print_json(i,secs,(id_data *) &uavs[i]);

      if ((uavs[i].lat_d)&&(uavs[i].base_lat_d)) {

        if (base_lat_d == 0.0) {

          base_lat_d  = uavs[i].base_lat_d;
          base_long_d = uavs[i].base_long_d;

          calc_m_per_deg(base_lat_d,base_long_d,&m_deg_lat,&m_deg_long);
        }

        y_m = (uavs[i].lat_d  - base_lat_d)  * m_deg_lat;
        x_m = (uavs[i].long_d - base_long_d) * m_deg_long;

#if TFT_DISPLAY
        y = TFT_HEIGHT - ((y_m / TRACK_SCALE) + (TFT_HEIGHT  / 2));
        x = ((x_m / TRACK_SCALE) + (TFT_WIDTH / 2));

        if ((y >= 0)&&(y < TFT_HEIGHT)&&(x >= 0)&&(x < TFT_WIDTH)) {

          tft.drawPixel(x,y,track_colours[i]);

          if (pixel_timestamp) {
            
            index = (y * TFT_WIDTH) + x;
            pixel_timestamp[index] = (uint16_t) secs;
          }
        }
#endif
      }

      uavs[i].flag = 0;

      last_json = msecs;
    }
  }

#if TFT_DISPLAY

  if (pixel_timestamp) {

    for (x = 0; x < TFT_WIDTH; ++x) {

      index = (clear_y * TFT_WIDTH) + x;

      if (pixel_timestamp[index]) {

        if ((j = (secs - (uint32_t) pixel_timestamp[index])) > TRACK_TIME) {

          pixel_timestamp[index] = 0;
          tft.drawPixel(x,clear_y,TFT_BLACK);
        }
      }
    }

    if (++clear_y >= TFT_HEIGHT) {

      clear_y = 0;
    }
  }
#endif

  if ((msecs - last_json) > 60000UL) { // Keep the serial link active

      print_json(MAX_UAVS,msecs / 1000,(id_data *) &uavs[MAX_UAVS]); 

      last_json = msecs;
  }

  //

  if (( msecs > DISPLAY_PAGE_MS)&&
      ((msecs - last_display_update) > 50)) {

    last_display_update = msecs;

    if ((msecs - last_page_change) >= DISPLAY_PAGE_MS) {

      for (i = 1; i < MAX_UAVS; ++i) {

        j = (display_uav + i) % MAX_UAVS;

        if (uavs[j].mac[0]) {

          display_uav = j;
          break;
        }
      }

      last_page_change += DISPLAY_PAGE_MS;
    }

#if (LCD_DISPLAY > 10) && (LCD_DISPLAY < 20) 

    switch (display_phase++) {

    case 0:

      if (uavs[display_uav].mac[0]) {

        sprintf(text,"%-16s",(char *) uavs[display_uav].op_id);
        u8x8.drawString(0,0,text);
      }
      break;

    case 1:

      if (uavs[display_uav].mac[0]) {

        sprintf(text,"%02x%02x%02x%02x%02x%02x %3d",
                uavs[display_uav].mac[0],uavs[display_uav].mac[1],uavs[display_uav].mac[2],
                uavs[display_uav].mac[3],uavs[display_uav].mac[4],uavs[display_uav].mac[5],
                uavs[display_uav].rssi);
        u8x8.drawString(0,1,text);
      }
      break;

    case 2:

      if (uavs[display_uav].mac[0]) {

        dtostrf(uavs[display_uav].lat_d,12,6,text1);
        sprintf(text,"%s %3d",text1,uavs[display_uav].altitude_msl);
        u8x8.drawString(0,2,text);
      }
      break;

    case 3:

      if (uavs[display_uav].mac[0]) {

        dtostrf(uavs[display_uav].long_d,12,6,text1);
        sprintf(text,"%s %3d",text1,uavs[display_uav].height_agl);
        u8x8.drawString(0,3,text);
      }
      break;

    case 4:

      if (uavs[display_uav].mac[0]) {

        dtostrf(uavs[display_uav].base_lat_d,12,6,text1);
        sprintf(text,"%s %3d",text1,uavs[display_uav].speed);
        u8x8.drawString(0,4,text);
      }
      break;

    case 5:

      if (uavs[display_uav].mac[0]) {

        dtostrf(uavs[display_uav].base_long_d,12,6,text1);
        sprintf(text,"%s %3d",text1,uavs[display_uav].heading);
        u8x8.drawString(0,5,text);
      }
      break;

    case 6:

      sprintf(text,"%06u",odid_wifi);
      u8x8.drawString(0,6,text);
      sprintf(text,"%06u",french_wifi);
      u8x8.drawString(0,7,text);
      break;

    case 7:

      sprintf(text,"%08x",(unsigned int) ESP.getFreeHeap());
      u8x8.drawString(8,6,text);
      sprintf(text,"%08x",(unsigned int) text);
      u8x8.drawString(8,7,text);
      break;

    default:

      display_phase = 0;
      break;      
    }    

    u8x8.refreshDisplay();

#endif
  }

  return;
}

/*
 *
 */

void print_json(int index,int secs,struct id_data *UAV) {

  char text[128], text1[16],text2[16], text3[16], text4[16];

  dtostrf(UAV->lat_d,11,6,text1);
  dtostrf(UAV->long_d,11,6,text2);
  dtostrf(UAV->base_lat_d,11,6,text3);
  dtostrf(UAV->base_long_d,11,6,text4);

  sprintf(text,"{ \"index\": %d, \"runtime\": %d, \"mac\": \"%02x:%02x:%02x:%02x:%02x:%02x\", ",
          index,secs,
          UAV->mac[0],UAV->mac[1],UAV->mac[2],UAV->mac[3],UAV->mac[4],UAV->mac[5]);
  Serial.print(text);
  sprintf(text,"\"operator\": \"%s\", \"uav latitude\": %s, \"uav longitude\": %s, \"alitude msl\": %d, ",
          UAV->op_id,text1,text2,UAV->altitude_msl);
  Serial.print(text);
  sprintf(text,"\"height agl\": %d, \"base latitude\": %s, \"base longitude\": %s, \"speed\": %d, \"heading\": %d }\r\n",
          UAV->height_agl,text3,text4,UAV->speed,UAV->heading);
  Serial.print(text);

  return;
}

/*
 *
 */

esp_err_t event_handler(void *ctx, system_event_t *event) {
  
  return ESP_OK;
}

/*
 *
 */

void callback(void* buffer, wifi_promiscuous_pkt_type_t type) {

  int                     length, typ, len, i, offset;
  char                    ssid_tmp[10], *a;
  uint8_t                *packet_u8, *payload, *val;
  wifi_promiscuous_pkt_t *packet;
  struct id_data         *UAV = NULL;
  static uint8_t          mac[6], nan_dest[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};

  a = NULL;
  
//

  ++callback_counter;

  memset(ssid_tmp,0,10);

  packet    = (wifi_promiscuous_pkt_t *) buffer;
  packet_u8 = (uint8_t *) buffer;
  
  payload   = packet->payload;
  length    = packet->rx_ctrl.sig_len;
  offset    = 36;

//

  for (i = 0; i < MAX_UAVS; ++i) {

    if (memcmp((void *) uavs[i].mac,&payload[10],6) == 0) {

      UAV = (struct id_data *) &uavs[i];
    }
  }

  if (!UAV) {

    for (i = 0; i < MAX_UAVS; ++i) {

      if (!uavs[i].mac[0]) {

        UAV = (struct id_data *) &uavs[i];
        break;
      }
    }    
  }

  if (!UAV) {

     UAV = (struct id_data *) &uavs[MAX_UAVS - 1];
  }

  memcpy(UAV->mac,&payload[10],6);

  UAV->rssi      = packet->rx_ctrl.rssi;
  UAV->last_seen = millis();

//

  if (memcmp(nan_dest,&payload[4],6) == 0) {

    // dump_frame(payload,length);

    if (odid_wifi_receive_message_pack_nan_action_frame((ODID_UAS_Data *) &UAS_data,(char *) mac,payload,length) == 0) {

      ++odid_wifi;

      if (UAS_data.OperatorIDValid) {

        UAV->flag = 1;
        strncpy((char *) UAV->op_id,(char *) UAS_data.OperatorID.OperatorId,ODID_ID_SIZE);
      }

      if (UAS_data.LocationValid) {

        UAV->flag         = 1;
        UAV->lat_d        = UAS_data.Location.Latitude;
        UAV->long_d       = UAS_data.Location.Longitude;
        UAV->altitude_msl = (int) UAS_data.Location.AltitudeGeo;
        UAV->height_agl   = (int) UAS_data.Location.Height;
        UAV->speed        = (int) UAS_data.Location.SpeedHorizontal;
        UAV->heading      = (int) UAS_data.Location.Direction;
      }

      if (UAS_data.SystemValid) {

        UAV->flag        = 1;
        UAV->base_lat_d  = UAS_data.System.OperatorLatitude;
        UAV->base_long_d = UAS_data.System.OperatorLongitude;
      }
    }

  } else if (payload[0] == 0x80) { // Beacon, look for a French ID.

    offset = 36;

    while (offset < length) {

      typ =  payload[offset];
      len =  payload[offset + 1];
      val = &payload[offset + 2];

      if ((typ    == 0xdd)&&
          (val[0] == 0x6a)&&
          (val[1] == 0x5c)&&
          (val[2] == 0x35)) {

        ++french_wifi;

        parse_french_id(UAV,&payload[offset]);

      } else if ((typ == 0)&&(!ssid_tmp[0])) {

        for (i = 0; (i < 8)&&(i < len); ++i) {

          ssid_tmp[i] = val[i];
        }
      }

      offset += len + 2;
    }

    if (ssid_tmp[0]) {

      strncpy((char *) ssid,ssid_tmp,8);
    }
#if 0
  } else if (a = (char *) memchr(payload,'G',length)) {

    if (memcmp(a,"GBR-OP-",7) == 0) {

      dump_frame(payload,length);     
    }
#endif
  }

  if ((!UAV->op_id[0])&&(!UAV->lat_d)) {

    UAV->mac[0] = 0;
  }

  return;
}

/*
 *
 */

void parse_french_id(struct id_data *UAV,uint8_t *payload) {

  int            length, i, j, l, t, index;
  uint8_t       *v;
  union {int32_t i32; uint32_t u32;}
                 uav_lat, uav_long, base_lat, base_long;
  union {int16_t i16; uint16_t u16;} 
                 alt, height;

  uav_lat.u32   = 
  uav_long.u32  = 
  base_lat.u32  =
  base_long.u32 = 0;

  alt.u16       =
  height.u16    = 0;

  index  = 0;
  length = payload[1];

  UAV->flag = 1;

  for (j = 6; j < length;) {

    t =  payload[j];
    l =  payload[j + 1];
    v = &payload[j + 2];

    switch (t) {

    case  1:

      if (v[0] != 1) {

        return;
      }
      
      break;

    case  2:

      for (i = 0; (i < (l - 6))&&(i < (ID_SIZE - 1)); ++i) {

        UAV->op_id[i] = (char) v[i + 6];
      }

      UAV->op_id[i] = 0;
      break;

    case  4:

      for (i = 0; i < 4; ++i) {

        uav_lat.u32 <<= 8;
        uav_lat.u32  |= v[i];
      }

      break;

    case  5:

      for (i = 0; i < 4; ++i) {

        uav_long.u32 <<= 8;
        uav_long.u32  |= v[i];
      }

      break;

    case  6:

      alt.u16 = (((uint16_t) v[0]) << 8) | (uint16_t) v[1];
      break;

    case  7:

      height.u16 = (((uint16_t) v[0]) << 8) | (uint16_t) v[1];
      break;

    case  8:

      for (i = 0; i < 4; ++i) {

        base_lat.u32 <<= 8;
        base_lat.u32  |= v[i];
      }

      break;

    case  9:

      for (i = 0; i < 4; ++i) {

        base_long.u32 <<= 8;
        base_long.u32  |= v[i];
      }

      break;

    case 10:

      UAV->speed = v[0];   
      break;

    case 11:

      UAV->heading = (((uint16_t) v[0]) << 8) | (uint16_t) v[1];
      break;

    default:
    
      break;
    }

    j += l + 2;
  }

  UAV->lat_d        = 1.0e-5 * (double) uav_lat.i32;
  UAV->long_d       = 1.0e-5 * (double) uav_long.i32;
  UAV->base_lat_d   = 1.0e-5 * (double) base_lat.i32;
  UAV->base_long_d  = 1.0e-5 * (double) base_long.i32;

  UAV->altitude_msl = alt.i16;
  UAV->height_agl   = height.i16;

  return;
}

/*
 *
 */

void dump_frame(uint8_t *frame,int length) {

  int      i;
  char     text[128], text2[20];

  text[0]     = 0;
  text2[0]    =
  text2[16]   = 0; 

  sprintf(text,"\r\nFrame, %d bytes\r\n   ",length);
  Serial.print(text);

  for (i = 0; i < 16; ++i) {

    sprintf(text,"%02d ",i);
    Serial.print(text);
  }

  Serial.print("\r\n 0 ");

  for (i = 0; i < (length + 4);) {

    sprintf(text,"%02x ",frame[i]);
    Serial.print(text);

    text2[i % 16] = ((frame[i] > 31)&&(frame[i] < 127)) ? frame[i]: '.';

    if ((++i % 16) == 0) {

      sprintf(text,"%s\r\n%2d ",text2,i / 16);
      Serial.print(text);          
    }

    text2[i % 16] = 0;
  }
    
  Serial.print("\r\n\r\n");          

  return;
}

/*
 *
 */

void calc_m_per_deg(double lat_d,double long_d,double *m_deg_lat,double *m_deg_long) {

  double pi, deg2rad, sin_lat, cos_lat, a, b, radius;

  pi       = 4.0 * atan(1.0);
  deg2rad  = pi / 180.0;

  sin_lat     = sin(lat_d * deg2rad);
  cos_lat     = cos(lat_d * deg2rad);
  a           = 0.08181922;
  b           = a * sin_lat;
  radius      = 6378137.0 * cos_lat / sqrt(1.0 - (b * b));
  *m_deg_long = deg2rad * radius;
  *m_deg_lat   = 111132.954 - (559.822 * cos(2.0 * lat_d * deg2rad)) - 
                (1.175 *  cos(4.0 * lat_d * deg2rad));

  return;
}

/*
 *
 */

/* -*- tab-width: 2; mode: c; -*-
 * 
 * Scanner for WiFi direct remote id. 
 * Handles both opendroneid and French formats.
 * 
 * Copyright (c) 2020-2021, Steve Jack.
 *
 * MIT licence.
 * 
 * Nov. '21     Added option to dump ODID frame to serial output.
 * Oct. '21     Updated for opendroneid release 1.0.
 * June '21     Added an option to log to an SD card.
 * May '21      Fixed a bug that presented when handing packed ODID data from multiple sources. 
 * April '21    Added support for EN 4709-002 WiFi beacons.
 * March '21    Added BLE scan. Doesn't work very well.
 * January '21  Added support for ANSI/CTA 2063 French IDs.
 *
 * Notes
 * 
 * May need a semaphore.
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
#define DUMP_ODID_FRAME    0

#define WIFI_SCAN          1
#define BLE_SCAN           0 // Experimental, does work very well.

#define SD_LOGGER          0
#define SD_CS              5
#define SD_LOGGER_LED      2

#define LCD_DISPLAY        0 // 11 for a SH1106 128X64 OLED.
#define DISPLAY_PAGE_MS 4000

#define TFT_DISPLAY        0
#define TFT_WIDTH        128
#define TFT_HEIGHT       160
#define TRACK_SCALE      1.0 // m/pixel
#define TRACK_TIME       120 // secs, 600

#define ID_SIZE     (ODID_ID_SIZE + 1)
#define MAX_UAVS           8
#define OP_DISPLAY_LIMIT  16

//

#if SD_LOGGER

#include <SD.h>
// #include <SdFat.h>

// #define SD_CONFIG       SdSpiConfig(SD_CS,DEDICATED_SPI,SD_SCK_MHZ(16))

#endif

//

#if BLE_SCAN

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAddress.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#endif

//

struct id_data {int       flag;
                uint8_t   mac[6];
                uint32_t  last_seen;
                char      op_id[ID_SIZE];
                char      uav_id[ID_SIZE];
                double    lat_d, long_d, base_lat_d, base_long_d;
                int       altitude_msl, height_agl, speed, heading, rssi;
};

#if SD_LOGGER
struct id_log  {int8_t    flushed;
                uint32_t  last_write;
                File      sd_log;
};
#endif

//

static void               print_json(int,int,struct id_data *);
static void               write_log(uint32_t,struct id_data *,struct id_log *);
static esp_err_t          event_handler(void *,system_event_t *);
static void               callback(void *,wifi_promiscuous_pkt_type_t);
static struct id_data    *next_uav(uint8_t *);
static void               parse_french_id(struct id_data *,uint8_t *);
static void               parse_odid(struct id_data *,ODID_UAS_Data *);
                        
static void               dump_frame(uint8_t *,int);
static void               calc_m_per_deg(double,double,double *,double *);
static char              *format_op_id(char *);

static double             base_lat_d = 0.0, base_long_d = 0.0, m_deg_lat = 110000.0, m_deg_long = 110000.0;
#if SD_LOGGER
static struct id_log      logfiles[MAX_UAVS + 1];
#endif

volatile char             ssid[10];
volatile unsigned int     callback_counter = 0, french_wifi = 0, odid_wifi = 0, odid_ble = 0;
volatile struct id_data   uavs[MAX_UAVS + 1];

volatile ODID_UAS_Data    UAS_data;

//

static const char        *title = "RID Scanner", *build_date = __DATE__,
                         *blank_latlong = " ---.------";

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

// For this library, the chip and pins are defined in User_Setup.h.
// Which is pretty horrible.

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
                  
static uint16_t *pixel_timestamp = NULL;
static uint32_t  track_colours[MAX_UAVS + 1];

#endif

#if BLE_SCAN

BLEScan *BLE_scan;
BLEUUID  service_uuid;

//

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  
    void onResult(BLEAdvertisedDevice device) {

      int                   i, k, len;
      char                  text[128];
      uint8_t              *payload, *odid, *mac;
      struct id_data       *UAV;
      ODID_BasicID_data     odid_basic;
      ODID_Location_data    odid_location;
      ODID_System_data      odid_system;
      ODID_OperatorID_data  odid_operator;

      text[0] = i = k = 0;
      
      //
      
      if ((len = device.getPayloadLength()) > 0) {

        BLEAddress ble_address = device.getAddress();
        mac                    = (uint8_t *) ble_address.getNative();

//      BLEUUID BLE_UUID = device.getServiceUUID(); // crashes program

        payload = device.getPayload();
        odid    = &payload[6];
#if 0
        for (i = 0, k = 0; i < ESP_BD_ADDR_LEN; ++i, k += 3) {

          sprintf(&text[k],"%02x ",mac[i]);
        }

        Serial.printf("%s\r\n",text);

        dump_frame(payload,payload[0]);      
#endif

        if ((payload[1] == 0x16)&&
            (payload[2] == 0xfa)&&
            (payload[3] == 0xff)&&
            (payload[4] == 0x0d)){

          UAV            = next_uav(mac);
          UAV->last_seen = millis();
          UAV->rssi      = device.getRSSI();
          UAV->flag      = 1;

          memcpy(UAV->mac,mac,6);

          switch (odid[0] & 0xf0) {

          case 0x00: // basic

            decodeBasicIDMessage(&odid_basic,(ODID_BasicID_encoded *) odid);
            break;

          case 0x10: // location
          
            decodeLocationMessage(&odid_location,(ODID_Location_encoded *) odid);
            UAV->lat_d        = odid_location.Latitude;
            UAV->long_d       = odid_location.Longitude;
            UAV->altitude_msl = (int) odid_location.AltitudeGeo;
            UAV->height_agl   = (int) odid_location.Height;
            UAV->speed        = (int) odid_location.SpeedHorizontal;
            UAV->heading      = (int) odid_location.Direction;
            break;

          case 0x40: // system

            decodeSystemMessage(&odid_system,(ODID_System_encoded *) odid);
            UAV->base_lat_d   = odid_system.OperatorLatitude;
            UAV->base_long_d  = odid_system.OperatorLongitude;
            break;

          case 0x50: // operator

            decodeOperatorIDMessage(&odid_operator,(ODID_OperatorID_encoded *) odid);
            strncpy((char *) UAV->op_id,(char *) odid_operator.OperatorId,ODID_ID_SIZE);
            break;
          }

          ++odid_ble;
        }
      }

      return;
    }
};

#endif

/*
 *
 */

void setup() {

  int         i;
  char        text[128];

  text[0] = i = 0;

  //

  memset((void *) &UAS_data,0,sizeof(ODID_UAS_Data));
  memset((void *) uavs,0,(MAX_UAVS + 1) * sizeof(struct id_data));
  memset((void *) ssid,0,10);

  strcpy((char *) uavs[MAX_UAVS].op_id,"NONE");

#if SD_LOGGER

  for (i = 0; i <= MAX_UAVS; ++i) {

    logfiles[i].flushed    = 1; 
    logfiles[i].last_write = 0;
  }

#endif

  //

  delay(100);

  Serial.begin(115200);

  Serial.printf("\r\n{ \"title\": \"%s\" }\r\n",title);
  Serial.printf("{ \"build date\": \"%s\" }\r\n",build_date);

  //

  nvs_flash_init();
  tcpip_adapter_init();

  esp_event_loop_init(event_handler,NULL);

#if WIFI_SCAN

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&callback); 

  // The channel should be 6.
  // If the second parameter is not WIFI_SECOND_CHAN_NONE, cast it to (wifi_second_chan_t).
  // There has been a report of the ESP not scanning the first channel if the second is set.
  
  esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE);

#endif

#if BLE_SCAN

  BLEDevice::init(title);

  service_uuid = BLEUUID("0000fffa-0000-1000-8000-00805f9b34fb");
  BLE_scan     = BLEDevice::getScan();

  BLE_scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  BLE_scan->setActiveScan(true); 
  BLE_scan->setInterval(100);
  BLE_scan->setWindow(99);  

#endif

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

#else
  blank_latlong;
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

#if 0

  const char *id[3] = {"OP-12345678901234567890", "GBR-OP-123456789012", "GBR-OP-12345678901234567890"};

  for (i = 0; i < 3; ++i) {

    sprintf(text,"\'%s\' -> \'%s\'\r\n",(char *) id[i],format_op_id((char *) id[i]));
    Serial.print(text);
  }

#endif

#if SD_LOGGER

  File root, file;

  pinMode(SD_LOGGER_LED,OUTPUT);
  digitalWrite(SD_LOGGER_LED,0);

  if (SD.begin(SD_CS)) {

    if (root = SD.open("/")) {

      while (file = root.openNextFile()) {

        sprintf(text,"{ \"file\": \"%s\", \"size\": %u }\r\n",file.name(),file.size());
        Serial.print(text);
        
        file.close();
      }

      root.close();
    }
  }

#endif

  Serial.print("{ \"message\": \"setup() complete\" }\r\n");

  return;
}

/*
 *
 */

void loop() {

  int             i, j, k, msl, agl;
  char            text[256];
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

  text[0] = i = j = k = 0;

  //
  
  msecs = millis();

#if BLE_SCAN

  uint32_t last_ble_scan = 0;

  if ((msecs - last_ble_scan) > 2000) {

    last_ble_scan = msecs;
  
    BLEScanResults foundDevices = BLE_scan->start(1,false);

    BLE_scan->clearResults(); 
  }
  
#endif

  msecs = millis();
  secs  = msecs / 1000;

  for (i = 0; i < MAX_UAVS; ++i) {

    if ((uavs[i].last_seen)&&
        ((msecs - uavs[i].last_seen) > 300000L)) {

      uavs[i].last_seen = 0;
      uavs[i].mac[0]    = 0;

#if SD_LOGGER
      if (logfiles[i].sd_log) {

        logfiles[i].sd_log.close();
        logfiles[i].flushed = 1;
      }
#endif
    }

    if (uavs[i].flag) {

      print_json(i,secs,(id_data *) &uavs[i]);

#if SD_LOGGER
      write_log(msecs,(id_data *) &uavs[i],&logfiles[i]);
#endif

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

#if SD_LOGGER

    if ((logfiles[i].sd_log)&&
        (!logfiles[i].flushed)&&
        ((msecs - logfiles[i].last_write) > 10000)) {

      digitalWrite(SD_LOGGER_LED,1);

      logfiles[i].sd_log.flush();
      logfiles[i].flushed = 1;

      logfiles[i].last_write = msecs;

      digitalWrite(SD_LOGGER_LED,0);
    }  

#endif
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

    msl = uavs[display_uav].altitude_msl;
    agl = uavs[display_uav].height_agl;

#if (LCD_DISPLAY > 10) && (LCD_DISPLAY < 20) 

    switch (display_phase++) {

    case 0:

      if (uavs[display_uav].mac[0]) {

        sprintf(text,"%-16s",format_op_id((char *) uavs[display_uav].op_id));
        u8x8.drawString(0,0,text);
      }
      break;

    case 1:

      if (uavs[display_uav].mac[0]) {

        if (uavs[display_uav].uav_id[0]) {

          for (i = 0; (i < 16)&&(uavs[display_uav].uav_id[i]); ++i) {

            text[i] = uavs[display_uav].uav_id[i];
          }

          while (i < 16) {

            text[i++] = ' ';
          }

          text[i] = 0;
          
        } else {

          sprintf(text,"%02x%02x%02x%02x%02x%02x %3d",
                  uavs[display_uav].mac[0],uavs[display_uav].mac[1],uavs[display_uav].mac[2],
                  uavs[display_uav].mac[3],uavs[display_uav].mac[4],uavs[display_uav].mac[5],
                  uavs[display_uav].rssi);
        }

        u8x8.drawString(0,1,text);
      }
      break;

    case 2:

      if (uavs[display_uav].mac[0]) {

        if ((uavs[display_uav].lat_d >= -90.0)&&
            (uavs[display_uav].lat_d <=  90.0)) {

          dtostrf(uavs[display_uav].lat_d,11,6,text1);
          u8x8.drawString(0,2,text1);
          
        } else {

          u8x8.drawString(0,2,blank_latlong);
        }

        if ((msl > -1000)&&(msl < 10000)) {

          sprintf(text," %4d",msl);
          u8x8.drawString(11,2,text);
          
        } else {

          u8x8.drawString(11,2," ----");
        }
      }
      break;

    case 3:

      if (uavs[display_uav].mac[0]) {

        if ((uavs[display_uav].long_d >= -180.0)&&
            (uavs[display_uav].long_d <=  180.0)) {

          dtostrf(uavs[display_uav].long_d,11,6,text1);
          u8x8.drawString(0,3,text1);
          
        } else {

          u8x8.drawString(0,3,blank_latlong);
        }

        if ((agl > -1000)&&(agl < 10000)) {

          sprintf(text," %4d",agl);
          u8x8.drawString(11,3,text);
          
        } else {

          u8x8.drawString(11,3," ----");
        }
      }
      break;

    case 4:

      if (uavs[display_uav].mac[0]) {

        if ((uavs[display_uav].base_lat_d >= -90.0)&&
            (uavs[display_uav].base_lat_d <=  90.0)) {

          dtostrf(uavs[display_uav].base_lat_d,11,6,text1);
          u8x8.drawString(0,4,text1);
          
        } else {

          u8x8.drawString(0,4,blank_latlong);
        }

        if ((uavs[display_uav].speed >= 0)&&(uavs[display_uav].speed < 10000)) {

          sprintf(text," %4d",uavs[display_uav].speed);
          u8x8.drawString(11,4,text);
          
        } else {

          u8x8.drawString(11,4," ----");
        }
      }
      break;

    case 5:

      if (uavs[display_uav].mac[0]) {

        if ((uavs[display_uav].base_long_d >= -180.0)&&
            (uavs[display_uav].base_long_d <=  180.0)) {

          dtostrf(uavs[display_uav].base_long_d,11,6,text1);
          u8x8.drawString(0,5,text1);
          
        } else {

          u8x8.drawString(0,5,blank_latlong);
        }

        if ((uavs[display_uav].heading >= 0)&&(uavs[display_uav].heading <= 360)) {

          sprintf(text," %4d",uavs[display_uav].heading);
          u8x8.drawString(11,5,text);
          
        } else {

          u8x8.drawString(11,5," ----");
        }
      }
      break;

    case 6:

      sprintf(text,"%06u",odid_wifi + odid_ble);
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
  sprintf(text,"\"id\": \"%s\", \"uav latitude\": %s, \"uav longitude\": %s, \"alitude msl\": %d, ",
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

void write_log(uint32_t msecs,struct id_data *UAV,struct id_log *logfile) {

#if SD_LOGGER

  int       secs, dsecs;
  char      text[128], filename[24], text1[16], text2[16];

  secs  = (int) (msecs / 1000);
  dsecs = ((short int) (msecs - (secs * 1000))) / 100;

  //

  if (!logfile->sd_log) {

    sprintf(filename,"/%02X%02X%02X%02X.TSV",
            UAV->mac[2],UAV->mac[3],UAV->mac[4],UAV->mac[5]);

    if (!(logfile->sd_log = SD.open(filename,FILE_APPEND))) {

      sprintf(text,"{ \"message\": \"Unable to open \'%s\'\" }\r\n",filename);
      Serial.print(text);
    }
  }

  //

  if (logfile->sd_log) {

    dtostrf(UAV->lat_d,11,6,text1);
    dtostrf(UAV->long_d,11,6,text2);

    sprintf(text,"%d.%d\t%s\t%s\t%s\t%s\t",
            secs,dsecs,UAV->op_id,UAV->uav_id,text1,text2);
    logfile->sd_log.print(text);

    sprintf(text,"%d\t%d\t%d\t",
            (int) UAV->altitude_msl,(int) UAV->speed,
            (int) UAV->heading);
    logfile->sd_log.print(text);

    logfile->sd_log.print("\r\n");
    logfile->flushed = 0;
  }
  
#endif

  return;
}

/*
 *
 */

esp_err_t event_handler(void *ctx, system_event_t *event) {
  
  return ESP_OK;
}

/*
 * This function handles WiFi packets.
 */

void callback(void* buffer,wifi_promiscuous_pkt_type_t type) {

  int                     length, typ, len, i, j, offset;
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

  UAV = next_uav(&payload[10]);

  memcpy(UAV->mac,&payload[10],6);

  UAV->rssi      = packet->rx_ctrl.rssi;
  UAV->last_seen = millis();

//

  if (memcmp(nan_dest,&payload[4],6) == 0) {

    // dump_frame(payload,length);

    if (odid_wifi_receive_message_pack_nan_action_frame((ODID_UAS_Data *) &UAS_data,(char *) mac,payload,length) == 0) {

      ++odid_wifi;

      parse_odid(UAV,(ODID_UAS_Data *) &UAS_data);
    }

  } else if (payload[0] == 0x80) { // beacon

    offset = 36;

    while (offset < length) {

      typ =  payload[offset];
      len =  payload[offset + 1];
      val = &payload[offset + 2];

      if ((typ    == 0xdd)&&
          (val[0] == 0x6a)&& // French
          (val[1] == 0x5c)&&
          (val[2] == 0x35)) {

        ++french_wifi;

        parse_french_id(UAV,&payload[offset]);

      } else if ((typ      == 0xdd)&&
                 (((val[0] == 0x90)&&(val[1] == 0x3a)&&(val[2] == 0xe6))|| // Parrot
                  ((val[0] == 0xfa)&&(val[1] == 0x0b)&&(val[2] == 0xbc)))) { // ODID

        ++odid_wifi;

        if ((j = offset + 7) < length) {

          memset((void *) &UAS_data,0,sizeof(UAS_data));
          
          odid_message_process_pack((ODID_UAS_Data *) &UAS_data,&payload[j],length - j);

#if DUMP_ODID_FRAME
          dump_frame(payload,length);     
#endif
          parse_odid(UAV,(ODID_UAS_Data *) &UAS_data);
        }

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

struct id_data *next_uav(uint8_t *mac) {

  int             i;
  struct id_data *UAV = NULL;

  for (i = 0; i < MAX_UAVS; ++i) {

    if (memcmp((void *) uavs[i].mac,mac,6) == 0) {

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

  return UAV;
}

/*
 *
 */

void parse_odid(struct id_data *UAV,ODID_UAS_Data *UAS_data2) {

  if (UAS_data2->BasicIDValid[0]) {

    UAV->flag = 1;
    strncpy((char *) UAV->uav_id,(char *) UAS_data2->BasicID[0].UASID,ODID_ID_SIZE);
  }

  if (UAS_data2->OperatorIDValid) {

    UAV->flag = 1;
    strncpy((char *) UAV->op_id,(char *) UAS_data2->OperatorID.OperatorId,ODID_ID_SIZE);
  }

  if (UAS_data2->LocationValid) {

    UAV->flag         = 1;
    UAV->lat_d        = UAS_data2->Location.Latitude;
    UAV->long_d       = UAS_data2->Location.Longitude;
    UAV->altitude_msl = (int) UAS_data2->Location.AltitudeGeo;
    UAV->height_agl   = (int) UAS_data2->Location.Height;
    UAV->speed        = (int) UAS_data2->Location.SpeedHorizontal;
    UAV->heading      = (int) UAS_data2->Location.Direction;
  }

  if (UAS_data2->SystemValid) {

    UAV->flag        = 1;
    UAV->base_lat_d  = UAS_data2->System.OperatorLatitude;
    UAV->base_long_d = UAS_data2->System.OperatorLongitude;
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

  uav_lat.u32  
  = 
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

    case  3:

      for (i = 0; (i < l)&&(i < (ID_SIZE - 1)); ++i) {

        UAV->uav_id[i] = (char) v[i];
      }

      UAV->uav_id[i] = 0;
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

char *format_op_id(char *op_id) {

  int           i, j, len;
  char         *a, *b;
  static char   short_id[OP_DISPLAY_LIMIT + 2];
  const char   *_op_ = "-OP-";

  strncpy(short_id,op_id,i = sizeof(short_id)); 
  
  short_id[OP_DISPLAY_LIMIT] = 0;

  if ((len = strlen(op_id)) > OP_DISPLAY_LIMIT) {

    if (a = strstr(short_id,_op_)) {

      b = strstr(op_id,_op_);
      j = strlen(a);

      strncpy(a,&b[3],j);
      short_id[OP_DISPLAY_LIMIT] = 0;
    }
  }

  return short_id;
}


/*
 *
 */ 

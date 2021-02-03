/* -*- tab-width: 2; mode: c; -*-
 * 
 * 
 */

#pragma GCC diagnostic warning "-Wunused-variable"

/*
 *
 */

#include <Arduino.h>

#include "utm.h"

UTM_Utilities utm_utils;

#if defined(ARDUINO_BLUEPILL_F103C8) || defined(ARDUINO_BLUEPILL_F103CB)

HardwareSerial Serial1(PA10,PA9);
HardwareSerial Serial2(PA3,PA2);
HardwareSerial Serial3(PB11,PB10);

#define SerialX Serial3

#else

#define SerialX Serial

#endif

/*
 * 
 */

void setup() {

  int                i, j, k;
  char               c, d, text[128], text2[32], text3[32];
  double             base_lat_d, base_long_d, m_deg_lat, m_deg_long;
  static const char *id  = "FIN87astrdge12k8", *secret = "xyz", *id2 = "abcd12345678xyz",
                    *id3 = "GBR13azertyuiopg", *secret3 = "abc";

  //
  
  SerialX.begin(115200);

  delay(1000);

  SerialX.print("\r\nTest UTM Utilities\r\n\n");

  //

  for (i = 0; i <= 36; ++i) {

    c = utm_utils.luhn36_i2c(i);
    j = utm_utils.luhn36_c2i(c);
    k = utm_utils.luhn36_c2i(d = toupper(c));

    sprintf(text,"%2d \'%c\' %2d \'%c\' %2d\r\n",i,(c) ? c: '?',j,d,k);
    SerialX.print(text);

    delay(25);
  }
  
  sprintf(text,"\r\n\'%s\' \'%s\' %d\r\n",
          id,secret,utm_utils.check_EU_op_id(id,secret));
  SerialX.print(text);

  sprintf(text,"\'%s\' \'%s\' %d\r\n",
          id3,secret3,utm_utils.check_EU_op_id(id3,secret3));
  SerialX.print(text);

#if 0

  c = utm_utils.luhn36_check(s);

  sprintf(text,"\'%s\' \'%c\'\r\n",
          utm_utils.s,c);
  SerialX.print(text);

#endif

  c = utm_utils.luhn36_check((char *) id2);

  sprintf(text,"\'%s\' \'%c\'\r\n",
          id2,c);
  SerialX.print(text);

  //

  SerialX.print("\r\nLat./Long.\r\n");

  utm_utils.calc_m_per_deg(base_lat_d = 54.33,base_long_d = -3.0,&m_deg_lat,&m_deg_long);

  dtostrf(base_lat_d,10,5,text2);
  dtostrf(base_long_d,10,5,text3);
  sprintf(text,"%s, %s, %6d m/deg lat., %6d m/deg long.\r\n",
          text2,text3,
          (int) (m_deg_lat + 0.5),(int) (m_deg_long + 0.5));
  SerialX.print(text);

  utm_utils.calc_m_per_deg(base_lat_d = 0.0,base_long_d,&m_deg_lat,&m_deg_long);

  dtostrf(base_lat_d,10,5,text2);
  dtostrf(base_long_d,10,5,text3);
  sprintf(text,"%s, %s, %6d m/deg lat., %6d m/deg long.\r\n",
          text2,text3,
          (int) (m_deg_lat + 0.5),(int) (m_deg_long + 0.5));
  SerialX.print(text);

  utm_utils.calc_m_per_deg(base_lat_d = 42.0,base_long_d,&m_deg_lat,&m_deg_long);

  dtostrf(base_lat_d,10,5,text2);
  dtostrf(base_long_d,10,5,text3);
  sprintf(text,"%s, %s, %6d m/deg lat., %6d m/deg long.\r\n",
          text2,text3,
          (int) (m_deg_lat + 0.5),(int) (m_deg_long + 0.5));
  SerialX.print(text);

  return;
}

/*
 * 
 */

void loop() {

  return;
}

/*
 *
 */

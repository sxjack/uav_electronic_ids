/* -*- tab-width: 2; mode: c; -*-
 * 
 * 
 * 
 */

#include <Arduino.h>

#include <id_open.h>

static ID_OpenDrone          squitter;

static struct UTM_parameters utm_parameters;
static struct UTM_data       utm_data;

void setup() {

  const char *auth = "I love the FAA and think that Remote ID is great. Honest.";

  Serial.begin(115200);

  memset(&utm_parameters,0,sizeof(utm_parameters));

  // strcpy(utm_parameters.UAS_operator,"USA");

  utm_parameters.UA_type = 2;
  utm_parameters.ID_type = 1;

  strcpy(utm_parameters.UAV_id,"MFR1L123456789ABC");
  
  squitter.init(&utm_parameters);
  
  squitter.set_auth((char *) auth);
  
  memset(&utm_data,0,sizeof(utm_data));

//  45 32 25.63 N 122 55 0.69 W

  utm_data.base_latitude  =   45.0 + (32.0 / 60.0) + (25.63 / 3600.0);
  utm_data.base_longitude = -122.0 - (55.0 / 60.0) - ( 0.69 / 3600.0);
  utm_data.base_alt_m     =   68.0;

  utm_data.latitude_d  =   45.0 + (32.0 / 60.0) + (17.42 / 3600.0);
  utm_data.longitude_d = -122.0 - (56.0 / 60.0) - (50.46 / 3600.0);
  utm_data.alt_msl_m   =  100.0;

  utm_data.satellites = 8;
  utm_data.base_valid = 1;

  return;
}

//

void loop() {

  squitter.transmit(&utm_data);

  return;
}

//

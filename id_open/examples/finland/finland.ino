/*
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

  memset(&utm_parameters,0,sizeof(utm_parameters));

  strcpy(utm_parameters.UAS_operator,"FIN-OP-1234567");

  utm_parameters.region      = 1;
  utm_parameters.EU_category = 2;

  squitter.init(&utm_parameters);
  
  memset(&utm_data,0,sizeof(utm_data));

  utm_data.base_latitude  = 61.0 + (26.0 / 60.0) + (48.51 / 3600.0);
  utm_data.base_longitude = 23.0 + (51.0 / 60.0) + (17.27 / 3600.0);
  utm_data.base_alt_m     = 150.0;

  utm_data.latitude_d  = 60.0 + (11.0 / 60.0) + (47.79 / 3600.0);
  utm_data.longitude_d = 24.0 + (56.0 / 60.0) + (42.37 / 3600.0);
  utm_data.alt_msl_m   = 200.0;

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

/*
 * 
 * 
 * 
 */

#include <Arduino.h>

#include <id_france.h>

static ID_France             squitter;

static struct UTM_parameters utm_parameters;
static struct UTM_data       utm_data;

void setup() {

  memset(&utm_parameters,0,sizeof(utm_parameters));

  strcpy(utm_parameters.UAS_operator,"FRA-OP-1234567");

  squitter.init(utm_parameters.UAS_operator);
  
  memset(&utm_data,0,sizeof(utm_data));

  utm_data.base_latitude  = 48.0 + (51.0 / 60.0) + (59.90 / 3600.0);
  utm_data.base_longitude =  2.0 + (22.0 / 60.0) + (35.97 / 3600.0);
  utm_data.base_alt_m     = 50.0;

  utm_data.latitude_d  = 48.0 + (49.0 / 60.0) + (59.08 / 3600.0);
  utm_data.longitude_d =  2.0 + (16.0 / 60.0) + (22.27 / 3600.0);
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

/* -*- tab-width: 2; mode: c; -*-
 *
 * UTM/eID interface structure definition and some defines.
 *
 * Copyright (c) 2020, Steve Jack.
 *
 */

#ifndef UTM_H
#define UTM_H

#define SATS_LEVEL_1   4
#define SATS_LEVEL_2   7
#define SATS_LEVEL_3  10

#define ID_SIZE       24

//

struct UTM_data {

  int    years;
  int    months;
  int    days;
  int    hours;
  int    minutes;
  int    seconds;
  int    csecs;
  double latitude_d;
  double longitude_d;
  float  alt_msl_m;
  float  alt_agl_m;
  int    speed_kn;
  int    heading;
  char  *hdop_s;
  char  *vdop_s;
  int    satellites;
  double base_latitude;
  double base_longitude;
  float  base_alt_m;
  int    base_valid;
};

#endif

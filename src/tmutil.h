/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_TMUTIL_H
#define __DADA_TMUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

  #include <time.h>

  /*! parse a string into struct tm; return equivalent time_t */
  time_t str2tm (struct tm* time, const char* str);

  /*! parse a string and return equivalent time_t */
  time_t str2time (const char* str);

  /*! parse a UTC string and return equivalent time_t */
  time_t str2utctime (const char* str);

  /*! parse a UTC time string into struct tm; return equivalent time_t */
  time_t str2utctm (struct tm* time, const char* str);

  /*! convert a UTC MJD time into the struct tm */
  time_t mjd2utctm (double mjd);

  /*! sleep for specified time in seconds via select syscall */
  void float_sleep (float seconds);

#ifdef __cplusplus
}
#endif

#endif // __DADA_TMUTIL_H

/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include <sys/time.h>

double diff_time ( struct timeval time1, struct timeval time2 )
{
  return ( (double)( time2.tv_sec - time1.tv_sec ) +
         ( (double)( time2.tv_usec - time1.tv_usec ) / 1000000.0 ) );
}

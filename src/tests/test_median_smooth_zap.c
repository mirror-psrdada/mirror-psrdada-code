/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#if 0

#undef TEMPLATE_TYPE
#define TEMPLATE_TYPE unsigned

#include "median_smooth_zap.h"

void test_unsigned ()
{
  unsigned* data = 0;
  unsigned ndat = 0;

  median_smooth_zap (ndat, data, 11);
}

#else

#undef TEMPLATE_TYPE
#define TEMPLATE_TYPE double

#include "median_smooth_zap.h"

void test_unsigned ()
{
  double* data = 0;
  unsigned ndat = 0;

  median_smooth_zap (ndat, data, 11);
}

#endif

int main ()
{
  return 0;
}

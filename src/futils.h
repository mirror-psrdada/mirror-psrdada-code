/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_FUTILS_H
#define __DADA_FUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

long filesize (const char* filename);
long fileread (const char* filename, char* buffer, unsigned bufsz);

#ifdef __cplusplus
}
#endif

#endif // __DADA_FUTILS_H

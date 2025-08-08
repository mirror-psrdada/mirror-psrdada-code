/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_DEAMON_H
#define __DADA_DEAMON_H

#ifdef __cplusplus
extern "C" {
#endif

  /* turn the calling process into a daemon */
  void be_a_daemon ();

  int be_a_daemon_with_log(char * logfile);

#ifdef __cplusplus
}
#endif

#endif // __DADA_DAEMON_H

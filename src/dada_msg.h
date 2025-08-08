/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_MSG_H
#define __DADA_MSG_H

#include <sys/types.h>
#include <sys/socket.h>

#ifdef MSG_NOSIGNAL
#define DADA_MSG_FLAGS MSG_NOSIGNAL | MSG_WAITALL
#else
#define DADA_MSG_FLAGS MSG_WAITALL
#endif

#include <pthread.h>

#ifdef PTHREAD_MUTEX_RECURSIVE
#define DADA_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE
#else
#define DADA_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

#endif // __DADA_MSG_H
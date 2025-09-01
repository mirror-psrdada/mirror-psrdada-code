/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_AFFINITY_H
#define __DADA_AFFINITY_H

/*
 * CPU Affinity library functions
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <sys/syscall.h>
#include <sys/mman.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

int dada_bind_thread_to_core(int core);

#ifdef __cplusplus
}
#endif

#endif // __DADA_AFFINITY_H

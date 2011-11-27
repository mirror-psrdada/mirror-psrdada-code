/***************************************************************************
 *  
 *    Copyright (C) 2011 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 * 
 ****************************************************************************/

/*
 * DADA CUDA library
 */

#ifndef __DADA_CUDA_H
#define __DADA_CUDA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dada_hdu.h"
#include "ipcbuf.h"
#include <cuda_runtime.h>

typedef enum { 
  PINNED, 
  PAGEABLE 
} memory_mode_t;

typedef struct
{
  cudaEvent_t start_event;
  cudaEvent_t stop_event;
  float elapsed;
} dada_cuda_profile_t;

int dada_cuda_get_device_count ();

int dada_cuda_select_device (int index);

char * dada_cuda_get_device_name (int index);

int dada_cuda_dbregister (dada_hdu_t * hdu);

int dada_cuda_dbunregister (dada_hdu_t * hdu);

void * dada_cuda_device_malloc (size_t bytes);

int dada_cuda_device_free (void * memory);

float dada_cuda_device_transfer ( void * from, void * to, size_t bufsz, memory_mode_t mode, dada_cuda_profile_t * timer);

int dada_cuda_profiler_init (dada_cuda_profile_t * timer);

#ifdef __cplusplus
}
#endif

#endif

/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_IPCUTIL_CUDA_H
#define __DADA_IPCUTIL_CUDA_H

#include <sys/types.h> // for key_t

/* ************************************************************************

   utilities for creation of shared memory on GPUs

   ************************************************************************ */

#ifdef __cplusplus
extern "C" {
#endif

  /* allocate size bytes in shared memory with the specified flags and key.
     returns the pointer to the base address and the shmid, id */
  void* ipc_alloc_cuda (key_t key, size_t size, int flag, int* id, void ** shm_addr, int device_id);

  int ipc_disconnect_cuda (void * devPtr);

  int ipc_dealloc_cuda (void * devPtr, int device_id);

  int ipc_zero_buffer_cuda (void * devPtr, size_t nbytes);

#ifdef __cplusplus
	   }
#endif

#endif // _DADA_IPCUTIL_CUDA_H

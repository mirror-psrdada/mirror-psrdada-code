/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#ifndef __DADA_IPCUTIL_H
#define __DADA_IPCUTIL_H

#include <sys/types.h>
#include <inttypes.h>

/*************************************************************************

   utilities for creation of shared memory and operations on semaphores

  ************************************************************************ */

#define IPCUTIL_PERM 0666 /* default: read/write permissions for all */

#ifdef __cplusplus
extern "C" {
#endif

  /* allocate size bytes in shared memory with the specified flags and key.
     returns the pointer to the base address and the shmid, id */
  void* ipc_alloc (key_t key, size_t size, int flag, int* id);

  /* operate on the specified semaphore */
  int ipc_semop (int semid, short num, short incr, short flag);

  /**
   * @brief operate on the specified semaphore, within the timeout.
   *
   * If the timeout is zero, then the ipc_semop function will be used.
   *
   * @param semid the semaphore identifier on which to operated
   * @param num the semaphore number
   * @param op the semaphore operation
   * @param flag the semaphore operation flags (e.g. IPC_NOWAIT, SEM_UNDO)
   * @param timeout the maximum timeout in nano seconds
   * @return -1 on operation error, 0 if the semaphore operation has completed within the time, +1 if the timeout occurred
   */
  int ipc_semtimedop (int semid, short num, short op, short flag, uint64_t timeout);

#ifdef __cplusplus
}
#endif

#endif // __DADA_IPCUTIL_H

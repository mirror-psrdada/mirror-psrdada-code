/***************************************************************************
 *  
 *    Copyright (C) 2024 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 * 
 ****************************************************************************/

#include "ipcio.h"
#include "dada_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

#include <sys/shm.h>
#include <sys/wait.h>
#include <assert.h>

//#define _DEBUG 1

int main (int argc, char** argv)
{
  key_t key = DADA_DEFAULT_BLOCK_KEY;
  int   arg;

  uint64_t nbufs = 16;
  uint64_t bufsz = getpagesize();
  ipcio_t rbuf_create = IPCIO_INIT;
  char verbose = 0;

  while ((arg = getopt(argc, argv, "hk:v")) != -1)
  {
    switch (arg)
    {
      case 'h':
        fprintf (stderr, 
          "test_ipcio_lifecycle [options]\n"
          " -v             verbose mode\n"
        );
        return 0;

    case 'k':
      if (sscanf (optarg, "%x", &key) != 1) {
        fprintf (stderr, "ERROR: could not parse key from %s\n",optarg);
        return EXIT_FAILURE;
      }
      break;

      case 'v':
        verbose++;
        break;
    }
  }

  uint64_t ring_buffer_size = nbufs * bufsz;
  uint64_t buffer_size = ring_buffer_size / 2;
  char * buffer_write = (char *) malloc(buffer_size);
  if (!buffer_write)
  {
    fprintf(stderr, "ERROR: could not allocate %lu bytes for buffer_write\n", buffer_size);
    return EXIT_FAILURE;
  }
  char * buffer_read  = (char *) malloc(buffer_size);
  if (!buffer_read)
  {
    fprintf(stderr, "ERROR: could not allocate %lu bytes for buffer_read\n", buffer_size);
    return EXIT_FAILURE;
  }

  for (unsigned i=0; i<buffer_size; i++)
  {
    buffer_write[i] = i % 128;
    buffer_read[i] = 0;
  }

  if (verbose)
    fprintf (stderr, "Creating shared memory ring buffer, nbufs=%"PRIu64" bufsz=%"PRIu64"\n", nbufs, bufsz);
  if (ipcio_create (&rbuf_create, key, nbufs, bufsz, 1) < 0)
  {
    fprintf (stderr, "ERROR: could not create shared memory ring buffer\n");
    return EXIT_FAILURE;
  }

  // writer that fills the buffer with data
  {
    ipcio_t rbuf_write = IPCIO_INIT;
    if (verbose)
      fprintf (stderr, "ipcio_connect (&rbuf_write, key)\n");
    if (ipcio_connect (&rbuf_write, key) < 0)
    {
      fprintf (stderr, "ERROR: could not connect to rbuf_write\n");
      return -1;
    }

    if (verbose)
      fprintf (stderr, "ipcio_open (&rbuf_write, key)\n");
    if (ipcio_open (&rbuf_write, 'w') < 0)
    {
      fprintf (stderr, "ERROR: ipcio_open failed on rbuf_write\n");
      return -1;
    }

    uint64_t sod_byte = 0;
    if (verbose)
      fprintf (stderr, "ipcio_start(&rbuf_write, %lu)\n", sod_byte);
    if (ipcio_start(&rbuf_write, sod_byte) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_start(&rbuf_write, %lu) failed\n", sod_byte);
      return EXIT_FAILURE;
    }

    if (verbose)
      fprintf (stderr, "ipcio_write (&rbuf_write, buffer_write, %lu)\n", buffer_size);
    if (ipcio_write(&rbuf_write, buffer_write, buffer_size) != buffer_size)
    {
      fprintf (stderr, "ERROR: ipcio_write failed for rbuf_write with buffer_size=%lu\n", buffer_size);
      return -1;
    }

    if (verbose)
      fprintf (stderr, "ipcio_stop(&rbuf_write)\n");
    if (ipcio_stop(&rbuf_write) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_stop(&rbuf_write) failed\n");
      return EXIT_FAILURE;
    }

    if (verbose)
      fprintf (stderr, "ipcio_close(&rbuf_write)\n");
    if (ipcio_close (&rbuf_write) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_close(&rbuf_write) failed\n");
      return EXIT_FAILURE;
    }

    if (verbose)
      fprintf (stderr, "ipcio_disconnect(&rbuf_write)\n");
    if (ipcio_disconnect(&rbuf_write) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_disconnect(&rbuf_write) failed\n");
      return EXIT_FAILURE;
    }
  }

  // read that drains the buffer of data
  {
    ipcio_t rbuf_read = IPCIO_INIT;
    if (verbose)
      fprintf (stderr, "ipcio_connect (&rbuf_read, key)\n");
    if (ipcio_connect (&rbuf_read, key) < 0)
    {
      fprintf (stderr, "ERROR: could not connect to rbuf_read\n");
      return -1;
    }

    if (verbose)
      fprintf (stderr, "ipcio_open (&rbuf_read, 'R')\n");
    if (ipcio_open (&rbuf_read, 'R') < 0)
    {
      fprintf (stderr, "ERROR: ipcio_open failed on rbuf_read\n");
      return -1;
    }

    if (verbose)
      fprintf (stderr, "ipcio_read (&rbuf_read, buffer_read, %lu)\n", buffer_size);
    if (ipcio_read(&rbuf_read, buffer_read, buffer_size) != buffer_size)
    {
      fprintf (stderr, "ERROR: buffer_read failed for rbuf_read with buffer_size=%lu\n", buffer_size);
      return -1;
    }

    uint64_t nerrors = 0;
    for (uint64_t i =0; i<buffer_size; i++)
    {
      if (buffer_read[i] != buffer_write[i])
      {
        nerrors++;
      }
    }

    if (verbose)
      fprintf (stderr, "ipcio_close (&rbuf_read)\n");
    if (ipcio_close (&rbuf_read) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_close(&rbuf_read) failed\n");
      return EXIT_FAILURE;
    }

    if (verbose)
      fprintf (stderr, "ipcio_disconnect (&rbuf_read)\n");
    if (ipcio_disconnect(&rbuf_read) < 0)
    {
      fprintf (stderr, "ERROR: ipcio_disconnect(&rbuf_read) failed\n");
      return EXIT_FAILURE;
    }
  }

  fprintf (stderr, "Scheduling IPC resources for destruction\n");
  if (ipcio_destroy (&rbuf_create) < 0)
  {
    fprintf (stderr, "ERROR: ipcio_disconnect(&rbuf_create) failed\n");
    return EXIT_FAILURE;
  }

  return 0;
}

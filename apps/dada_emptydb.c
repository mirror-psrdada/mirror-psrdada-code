/***************************************************************************
 *
 *    Copyright (C) 2024-2025 by Andrew Jameson
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_client.h"
#include "dada_hdu.h"
#include "dada_def.h"
#include "dada_generator.h"
#include "ascii_header.h"
#include "tmutil.h"
#include "futils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>

void usage()
{
  fprintf (stdout,
	 "dada_emptydb [options] header_file\n"
     "  writes header but zero bytes to the ring buffer\n"
     " -k         hexadecimal shared memory key  [default: %x]\n"
     " -h         print help\n",
     DADA_DEFAULT_BLOCK_KEY);
}

#define _DEBUG

int main (int argc, char **argv)
{
  /* DADA Header plus Data Unit */
  dada_hdu_t* hdu = 0;

  /* DADA Logger */
  multilog_t* log = 0;

  /* header to use for the data block */
  char * header_file = 0;

  /* Flag set in verbose mode */
  char verbose = 0;

  /* hexadecimal shared memory key */
  key_t dada_key = DADA_DEFAULT_BLOCK_KEY;

  int arg = 0;

  while ((arg=getopt(argc,argv,"hk:v")) != -1)
    switch (arg) {

    case 'h':
      usage();
      return (EXIT_SUCCESS);

    case 'k':
      if (sscanf (optarg, "%x", &dada_key) != 1) {
        fprintf (stderr, "ERROR: could not parse key from %s\n",optarg);
        usage();
        return EXIT_FAILURE;
      }
      break;

    case 'v':
      verbose++;
      break;

    default:
      usage ();
      return EXIT_FAILURE;
    }

  if ((argc - optind) != 1) {
    fprintf (stderr, "Error: a header file must be specified\n");
    usage();
    exit(EXIT_FAILURE);
  }

  header_file = strdup(argv[optind]);

  log = multilog_open ("dada_emptydb", 0);

  multilog_add (log, stderr);

  hdu = dada_hdu_create (log);

  dada_hdu_set_key(hdu, dada_key);

  fprintf(stderr, "main: dada_hdu_connect (hdu)\n");
  if (dada_hdu_connect (hdu) < 0)
    return EXIT_FAILURE;

  fprintf(stderr, "main: dada_hdu_lock_write (hdu)\n");
  if (dada_hdu_lock_write (hdu) < 0)
    return EXIT_FAILURE;

  fprintf(stderr, "main: ipcbuf_get_next_write (hdu->header_block)\n");
  hdu->header = ipcbuf_get_next_write (hdu->header_block);
  if (!hdu->header)  {
    multilog (log, LOG_ERR, "Could not get next header block\n");
    return EXIT_FAILURE;
  }
  hdu->header_size = ipcbuf_get_bufsz (hdu->header_block);

  fprintf(stderr, "main: hdu->header=%p hdu->header_size=%lu\n", hdu->header, hdu->header_size);
  if (fileread (header_file, hdu->header, hdu->header_size) < 0) {
    multilog (log, LOG_ERR, "Could not read header from %s\n", header_file);
  }

  // Enable EOD so that subsequent transfers will move to the next buffer in the header block
  if (ipcbuf_enable_eod(hdu->header_block) < 0)
  {
    multilog (log, LOG_ERR, "Could not enable EOD on Header Block\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "main: ipcbuf_mark_filled(hdu->header_block, %lu)\n", hdu->header_size);
  if (ipcbuf_mark_filled (hdu->header_block, hdu->header_size) < 0)  {
    multilog (log, LOG_ERR, "Could not mark filled Header Block\n");
    return EXIT_FAILURE;
  }

  // signals the end of data on the data block
  if (ipcio_close (hdu->data_block) < 0)  {
    multilog (log, LOG_ERR, "Could not close Data Block\n");
    return EXIT_FAILURE;
  }

  fprintf(stderr, "main: dada_hdu_unlock_write (hdu)\n");
  if (dada_hdu_unlock_write (hdu) < 0)
    return EXIT_FAILURE;

  fprintf(stderr, "main: dada_hdu_disconnect (hdu)\n");
  if (dada_hdu_disconnect (hdu) < 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}


/***************************************************************************
 *
 *    Copyright (C) 2010-2025 by Andrew Jameson and Willem van Straten
 *    Licensed under the Academic Free License version 2.1
 *
 ****************************************************************************/

#include "dada_hdu.h"
#include "dada_def.h"
#include "ipcbuf.h"

#include "node_array.h"
#include "string_array.h"
#include "ascii_header.h"
#include "daemon.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

void usage()
{
  fprintf (stdout,
     "dada_dbxferinfo [options]\n"
     " -k         hexadecimal shared memory key  [default: %x]\n"
     " -v         be verbose\n"
     " -d         run as daemon\n", DADA_DEFAULT_BLOCK_KEY);
}

const char * state_to_str(int state);
void print_ipcbuf_info (ipcbuf_t * db);

int main (int argc, char **argv)
{

  /* DADA Header plus Data Unit */
  dada_hdu_t* hdu = 0;

  /* DADA Logger */
  multilog_t* log = 0;

  /* Flag set in daemon mode */
  char daemon = 0;

  /* Flag set in verbose mode */
  char verbose = 0;

  /* hexadecimal shared memory key */
  key_t dada_key = DADA_DEFAULT_BLOCK_KEY;

  int arg = 0;

  /* TODO the amount to conduct a busy sleep in between clearing each sub
   * block */

  while ((arg=getopt(argc,argv,"k:dv")) != -1)
    switch (arg) {

    case 'k':
      if (sscanf (optarg, "%x", &dada_key) != 1) {
        fprintf (stderr,"dada_dbxferinfo: could not parse key from %s\n",
                         optarg);
        return -1;
      }
      break;

    case 'd':
      daemon=1;
      break;

    case 'v':
      verbose=1;
      break;

    default:
      usage ();
      return 0;

    }

  log = multilog_open ("dada_dbmonitor", daemon);

  if (daemon)
    be_a_daemon ();
  else
    multilog_add (log, stderr);

  multilog_serve (log, DADA_DEFAULT_DBMONITOR_LOG);

  hdu = dada_hdu_create (log);

  dada_hdu_set_key(hdu, dada_key);

  if (verbose)
    printf("Connecting to data block\n");
  if (dada_hdu_connect (hdu) < 0)
    return EXIT_FAILURE;

  /* get a pointer to the data block */

  ipcbuf_t *hb = hdu->header_block;
  ipcbuf_t *db = (ipcbuf_t *) hdu->data_block;

  uint64_t bufsz = ipcbuf_get_bufsz (hb);
  uint64_t nhbufs = ipcbuf_get_nbufs (hb);
  uint64_t total_bytes = nhbufs * bufsz;
  int nreaders = ipcbuf_get_nreaders (hb);

  if (verbose)
  {
    fprintf(stderr,"HEADER BLOCK:\n");
    print_ipcbuf_info(hb);
  }

  fprintf(stderr,"DATA BLOCK:\n");
  print_ipcbuf_info(db);

  if (dada_hdu_disconnect (hdu) < 0)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

void print_ipcbuf_info (ipcbuf_t * db)
{
  uint64_t bufsz = ipcbuf_get_bufsz (db);
  uint64_t nbufs = ipcbuf_get_nbufs (db);
  uint64_t total_bytes = nbufs * bufsz;
  int nreaders = ipcbuf_get_nreaders (db);
  double memory_used =  ((double) total_bytes) / (1024.0*1024.0);

  fprintf(stderr,"Number of buffers: %"PRIu64"\n",nbufs);
  fprintf(stderr,"Buffer size: %"PRIu64"\n",bufsz);
  fprintf(stderr,"Total buffer memory: %5.0f MB\n", memory_used);
  fprintf(stderr,"Number of readers: %d\n", nreaders);

  fprintf(stderr, "\n");
  fprintf(stderr, "sync->w_buf_curr: %"PRIu64"\n", db->sync->w_buf_curr);
  fprintf(stderr, "sync->w_buf_next: %"PRIu64"\n", db->sync->w_buf_next);
  fprintf(stderr, "sync->w_state:    %s\n", state_to_str(db->sync->w_state));

  fprintf(stderr, "Reader\tr_buf\tSOD\tEOD\tRSEM\tCONN\tFULL\tCLEAR\tr_state\n");
  for (int iread=0; iread < nreaders; iread++)
  {
    fprintf (stderr, "%d\t%"PRIu64"\t%"PRIu64"\t%"PRIu64"\t%d\t%d\t%"PRIu64"\t%"PRIu64"\t%s\n",
              iread,
              db->sync->r_bufs[iread],
              ipcbuf_get_sodack_iread(db, iread),
              ipcbuf_get_eodack_iread(db, iread),
              ipcbuf_get_read_semaphore_count (db),
              ipcbuf_get_reader_conn_iread (db, iread),
              ipcbuf_get_nfull_iread(db, iread),
              ipcbuf_get_nclear_iread(db, iread),
              state_to_str(db->sync->r_states[iread]));
  }

  int i=0;

  fprintf(stderr, "\n");
  fprintf(stderr, "           START              END            EOD\n");
  fprintf(stderr, "ID [    buf,   byte]    [    buf,   byte]   FLAG\n");
  fprintf(stderr, "=================================================\n");
  for (int i=0;i<IPCBUF_XFERS;i++) {
    fprintf(stderr,"%2d [%7"PRIu64",%7"PRIu64"] => [%7"PRIu64",%7"PRIu64"]   %4d",
                    i, db->sync->s_buf[i],db->sync->s_byte[i],
                    db->sync->e_buf[i],db->sync->e_byte[i],
                    db->sync->eod[i]);

    if (i == db->sync->w_xfer)
      fprintf(stderr, " W");
    else
      fprintf(stderr, "  ");
    for (int iread=0; iread < nreaders; iread++)
      if (i == db->sync->r_xfers[iread] % IPCBUF_XFERS)
        fprintf(stderr, " R%d", iread);
      else
        fprintf(stderr, "  ");

    fprintf(stderr,"\n");
  }
}

const char * state_to_str(int state)
{

  switch (state)
  {
    case 0:
      return "disconnected";

    case 1:
      return "connected";

    case 2:
      return "one process that writes to the buffer";

    case 3:
      return "start-of-data flag has been raised";

    case 4:
      return "next operation will change writing state";

    case 5:
      return "one process that reads from the buffer";

    case 6:
      return "start-of-data flag has been raised";

    case 7:
      return "end-of-data flag has been raised";

    case 8:
      return "currently viewing";

    case 9:
      return "end-of-data while viewer";

    default:
      return "unknown";

  }

}

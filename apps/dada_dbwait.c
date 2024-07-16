#include "dada_hdu.h"
#include "dada_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void usage()
{
  fprintf(stdout,
    "dada_dbwait [options]\n"
    " -k         hexadecimal shared memory key [default: %x]\n"
    " -t tag     unused command line option\n"
    " -w         wait until the data block is created\n",
    DADA_DEFAULT_BLOCK_KEY);
}

int main (int argc, char **argv)
{
  /* DADA Header plus Data Unit */
  dada_hdu_t* hdu = 0;

  /* DADA Logger */
  multilog_t* log = 0;

  /* dada key for SHM */
  key_t dada_key = DADA_DEFAULT_BLOCK_KEY;

  /* flag to wait for the data block to be created */
  int wait = 0;

  int arg = 0;

  while ((arg=getopt(argc,argv,"k:t:w")) != -1)
    switch (arg)
    {
    case 't':
      break;

    case 'k':
      if (sscanf(optarg, "%x", &dada_key) != 1)
      {
        fprintf(stderr, "dada_header: could not parse key from %s\n", optarg);
        return -1;
      }
      break;

    case 'w':
      wait = 1;
      break;

    default:
      usage();
      return 0;
    }

  hdu = dada_hdu_create(NULL);

  // Set the particular dada key
  dada_hdu_set_key(hdu, dada_key);

  int connected = dada_hdu_connect(hdu);
  while (wait && connected == -1)
  {
    if (connected == -1)
    {
      sleep(1);
    }
    connected = dada_hdu_connect(hdu);
  }

  // if the data block did not exist, return 1
  if (connected == -1)
  {
    return 0;
  }
  else
  {
    fprintf(stdout, "%x exists\n", dada_key);
  }

  if (dada_hdu_disconnect(hdu) < 0)
  {
    return -1;
  }

  return 0;
}


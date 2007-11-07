#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipcio.h"

// #define _DEBUG 1

void ipcio_init (ipcio_t* ipc)
{
  ipc -> bytes = 0;
  ipc -> rdwrt = 0;
  ipc -> curbuf = 0;

  ipc -> marked_filled = 0;

  ipc -> sod_pending = 0;
  ipc -> sod_buf  = 0;
  ipc -> sod_byte = 0;
}

/* create a new shared memory block and initialize an ipcio_t struct */
int ipcio_create (ipcio_t* ipc, key_t key, uint64_t nbufs, uint64_t bufsz)
{
  if (ipcbuf_create ((ipcbuf_t*)ipc, key, nbufs, bufsz) < 0) {
    fprintf (stderr, "ipcio_create: ipcbuf_create error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

/* connect to an already created ipcbuf_t struct in shared memory */
int ipcio_connect (ipcio_t* ipc, key_t key)
{
  if (ipcbuf_connect ((ipcbuf_t*)ipc, key) < 0) {
    fprintf (stderr, "ipcio_connect: ipcbuf_connect error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

/* disconnect from an already connected ipcbuf_t struct in shared memory */
int ipcio_disconnect (ipcio_t* ipc)
{
  if (ipcbuf_disconnect ((ipcbuf_t*)ipc) < 0) {
    fprintf (stderr, "ipcio_disconnect: ipcbuf_disconnect error\n");
    return -1;
  }
  ipcio_init (ipc);
  return 0;
}

int ipcio_destroy (ipcio_t* ipc)
{
  ipcio_init (ipc);
  return ipcbuf_destroy ((ipcbuf_t*)ipc);
}

/* start reading/writing to an ipcbuf */
int ipcio_open (ipcio_t* ipc, char rdwrt)
{
  if (rdwrt != 'R' && rdwrt != 'r' && rdwrt != 'w' && rdwrt != 'W') {
    fprintf (stderr, "ipcio_open: invalid rdwrt = '%c'\n", rdwrt);
    return -1;
  }

  ipc -> rdwrt = 0;
  ipc -> bytes = 0;
  ipc -> curbuf = 0;

  if (rdwrt == 'w' || rdwrt == 'W') {

    /* read from file, write to shm */
    if (ipcbuf_lock_write ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_lock_write\n");
      return -1;
    }

    if (rdwrt == 'w' && ipcbuf_disable_sod((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_disable_sod\n");
      return -1;
    }

    ipc -> rdwrt = rdwrt;
    return 0;

  }

  if (rdwrt == 'R') {
    if (ipcbuf_lock_read ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_open: error ipcbuf_lock_read\n");
      return -1;
    }
  }

  ipc -> rdwrt = rdwrt;
  return 0;
}

uint64_t ipcio_get_start_minimum (ipcio_t* ipc)
{
  uint64_t bufsz  = ipcbuf_get_bufsz ((ipcbuf_t*)ipc);
  uint64_t minbuf = ipcbuf_get_sod_minbuf ((ipcbuf_t*)ipc);
  return minbuf * bufsz;
}


/* Checks if a SOD request has been requested, if it has, then it enables
 * SOD */
int ipcio_check_pending_sod (ipcio_t* ipc)
{
  ipcbuf_t* buf = (ipcbuf_t*) ipc;

  /* If the SOD flag has not been raised return 0 */
  if (ipc->sod_pending == 0) 
    return 0;

  /* The the buffer we wish to raise SOD on has not yet been written, then
   * don't raise SOD */
  if (ipcbuf_get_write_count (buf) <= ipc->sod_buf)
    return 0;

  /* Try to enable start of data on the sod_buf & sod byte */
  if (ipcbuf_enable_sod (buf, ipc->sod_buf, ipc->sod_byte) < 0) {
    fprintf (stderr, "ipcio_check_pendind_sod: fail ipcbuf_enable_sod\n");
    return -1;
  }
  
  ipc->sod_pending = 0;
  return 0;
}

/* start writing valid data to an ipcbuf. byte is the absolute byte relative to
 * the start of the data block */
int ipcio_start (ipcio_t* ipc, uint64_t byte)
{
  ipcbuf_t* buf  = (ipcbuf_t*) ipc;
  uint64_t bufsz = ipcbuf_get_bufsz (buf);

  if (ipc->rdwrt != 'w') {
    fprintf (stderr, "ipcio_start: invalid ipcio_t (%c)\n",ipc->rdwrt);
    return -1;
  }

  ipc->sod_buf  = byte / bufsz;
  ipc->sod_byte = byte % bufsz;
  ipc->sod_pending = 1;
  ipc->rdwrt = 'W';

  return ipcio_check_pending_sod (ipc);
}

/* stop reading/writing to an ipcbuf */
int ipcio_stop_close (ipcio_t* ipc, char unlock)
{
  if (ipc -> rdwrt == 'W') {

#ifdef _DEBUG
    if (ipc->curbuf)
      fprintf (stderr, "ipcio_close:W buffer:%"PRIu64" %"PRIu64" bytes. "
                       "buf[0]=%x\n", ipc->buf.sync->w_buf, ipc->bytes, 
                       ipc->curbuf[0]);
#endif

    if (ipcbuf_is_writing((ipcbuf_t*)ipc)) {

      if (ipcbuf_enable_eod ((ipcbuf_t*)ipc) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcbuf_enable_eod\n");
        return -1;
      }

      if (ipcbuf_mark_filled ((ipcbuf_t*)ipc, ipc->bytes) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcbuf_mark_filled\n");
        return -1;
      }

      if (ipcio_check_pending_sod (ipc) < 0) {
        fprintf (stderr, "ipcio_close:W error ipcio_check_pending_sod\n");
        return -1;
      }

      /* Ensure that mark_filled is not called again for this buffer
         in ipcio_write */
      ipc->marked_filled = 1;

      if (ipc->bytes == ipcbuf_get_bufsz((ipcbuf_t*)ipc)) {
#ifdef _DEBUG
        fprintf (stderr, "ipcio_close:W last buffer was filled\n");
#endif
        ipc->curbuf = 0;
      }

    }

    ipc->rdwrt = 'w';

    if (!unlock)
      return 0;

  }

  if (ipc -> rdwrt == 'w') {

    /* Removed to allow a writer to write more than one transfer to the 
     * data block */
    /*
#ifdef _DEBUG
    fprintf (stderr, "ipcio_close:W calling ipcbuf_reset\n");
#endif

    if (ipcbuf_reset ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:W error ipcbuf_reset\n");
      return -1;
    }*/

    if (ipc->buf.sync->w_xfer > 0) {

      uint64_t prev_xfer = ipc->buf.sync->w_xfer - 1;
      /* Ensure the w_buf pointer is pointing buffer after the 
       * most recent EOD */
      ipc->buf.sync->w_buf = ipc->buf.sync->e_buf[prev_xfer % IPCBUF_XFERS]+1;

      // TODO CHECK IF WE NEED TO DECREMENT the count??
      
    }

    if (ipcbuf_unlock_write ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:W error ipcbuf_unlock_write\n");
      return -1;
    }

    ipc -> rdwrt = 0;
    return 0;

  }

  if (ipc -> rdwrt == 'R') {

    if (ipcbuf_unlock_read ((ipcbuf_t*)ipc) < 0) {
      fprintf (stderr, "ipcio_close:R error ipcbuf_unlock_read\n");
      return -1;
    }

    ipc -> rdwrt = 0;
    return 0;

  }

  fprintf (stderr, "ipcio_close: invalid ipcio_t\n");
  return -1;
}

/* stop writing valid data to an ipcbuf */
int ipcio_stop (ipcio_t* ipc)
{
  if (ipc->rdwrt != 'W') {
    fprintf (stderr, "ipcio_stop: not writing!\n");
    return -1;
  }
  return ipcio_stop_close (ipc, 0);
}

/* stop reading/writing to an ipcbuf */
int ipcio_close (ipcio_t* ipc)
{
  return ipcio_stop_close (ipc, 1);
}

/* return 1 if the ipcio is open for reading or writing */
int ipcio_is_open (ipcio_t* ipc)
{
  char rdwrt = ipc->rdwrt;
  return rdwrt == 'R' || rdwrt == 'r' || rdwrt == 'w' || rdwrt == 'W';
}

/* write bytes to ipcbuf */
ssize_t ipcio_write (ipcio_t* ipc, char* ptr, size_t bytes)
{

  size_t space = 0;
  size_t towrite = bytes;

  if (ipc->rdwrt != 'W' && ipc->rdwrt != 'w') {
    fprintf (stderr, "ipcio_write: invalid ipcio_t\n");
    return -1;
  }

  while (bytes) {

    /*
      The check for a full buffer is done at the start of the loop
      so that if ipcio_write exactly fills a buffer before exiting,
      the end-of-data flag can be raised before marking the buffer
      as filled in ipcio_stop
    */
    if (ipc->bytes == ipcbuf_get_bufsz((ipcbuf_t*)ipc)) {

      if (!ipc->marked_filled) {

#ifdef _DEBUG
        fprintf (stderr, "ipcio_write buffer:%"PRIu64" mark_filled\n",
                 ipc->buf.sync->w_buf);
#endif
        
        /* the buffer has been filled */
        if (ipcbuf_mark_filled ((ipcbuf_t*)ipc, ipc->bytes) < 0) {
          fprintf (stderr, "ipcio_write: error ipcbuf_mark_filled\n");
          return -1;
        }

        if (ipcio_check_pending_sod (ipc) < 0) {
          fprintf (stderr, "ipcio_write: error ipcio_check_pending_sod\n");
          return -1;
        }

      }

      ipc->curbuf = 0;
      ipc->bytes = 0;
      ipc->marked_filled = 1;

    }

    if (!ipc->curbuf) {

#ifdef _DEBUG
      fprintf (stderr, "ipcio_write buffer:%"PRIu64" ipcbuf_get_next_write\n",
               ipc->buf.sync->w_buf);
#endif

      ipc->curbuf = ipcbuf_get_next_write ((ipcbuf_t*)ipc);

      if (!ipc->curbuf) {
        fprintf (stderr, "ipcio_write: ipcbuf_next_write\n");
        return -1;
      }

      ipc->marked_filled = 0;
      ipc->bytes = 0;
    }

    space = ipcbuf_get_bufsz((ipcbuf_t*)ipc) - ipc->bytes;
    if (space > bytes)
      space = bytes;

    if (space > 0) {

#ifdef _DEBUG
      fprintf (stderr, "ipcio_write buffer:%"PRIu64" offset:%"PRIu64
               " count=%u\n", ipc->buf.sync->w_buf, ipc->bytes, space);
#endif

      memcpy (ipc->curbuf + ipc->bytes, ptr, space);
      ipc->bytes += space;
      ptr += space;
      bytes -= space;
    }

  }

  return towrite;
}


/* read bytes from ipcbuf */
ssize_t ipcio_read (ipcio_t* ipc, char* ptr, size_t bytes)
{
  size_t space = 0;
  size_t toread = bytes;

  if (ipc -> rdwrt != 'r' && ipc -> rdwrt != 'R') {
    fprintf (stderr, "ipcio_read: invalid ipcio_t\n");
    return -1;
  }

  while (!ipcbuf_eod((ipcbuf_t*)ipc)) {

    if (!ipc->curbuf) {

      ipc->curbuf = ipcbuf_get_next_read ((ipcbuf_t*)ipc, &(ipc->curbufsz));

#ifdef _DEBUG
      fprintf (stderr, "ipcio_read buffer:%"PRIu64" %"PRIu64" bytes. buf[0]=%x\n",
               ipc->buf.sync->r_buf, ipc->curbufsz, ipc->curbuf[0]);
#endif

      if (!ipc->curbuf) {
        fprintf (stderr, "ipcio_read: error ipcbuf_next_read\n");
        return -1;
      }

      ipc->bytes = 0;

    }

    if (bytes)  {

      space = ipc->curbufsz - ipc->bytes;
      if (space > bytes)
        space = bytes;

      memcpy (ptr, ipc->curbuf + ipc->bytes, space);
      
      ipc->bytes += space;
      ptr += space;
      bytes -= space;

    }

    if (ipc->bytes == ipc->curbufsz) {

      if (ipc -> rdwrt == 'R' && ipcbuf_mark_cleared ((ipcbuf_t*)ipc) < 0) {
        fprintf (stderr, "ipcio_write: error ipcbuf_mark_filled\n");
        return -1;
      }

      ipc->curbuf = 0;
      ipc->bytes = 0;

    }
    else if (!bytes)
      break;

  }

  return toread - bytes;
}

uint64_t ipcio_tell (ipcio_t* ipc)
{
  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t*)ipc);
  uint64_t nbuf = 0;

  if (ipc -> rdwrt == 'R' || ipc -> rdwrt == 'r')
    nbuf = ipcbuf_get_read_count ((ipcbuf_t*)ipc);
  else if (ipc -> rdwrt == 'W' || ipc -> rdwrt == 'w')
    nbuf = ipcbuf_get_write_count ((ipcbuf_t*)ipc);

  return nbuf * bufsz + ipc->bytes;
}



int64_t ipcio_seek (ipcio_t* ipc, int64_t offset, int whence)
{
  /* the current absolute byte count position in the ring buffer */
  uint64_t current = 0;
  /* the absolute value of the offset */
  uint64_t abs_offset = 0;
  /* space left in the current buffer */
  uint64_t space = 0;
  /* end of current buffer flag */
  int eobuf = 0;

  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t*)ipc);
  uint64_t nbuf = ipcbuf_get_read_count ((ipcbuf_t*)ipc);
  if (nbuf > 0)
    nbuf -= 1;

  current = bufsz * nbuf + ipc->bytes;

  if (whence == SEEK_SET)
    offset -= current;

  if (offset < 0) {
    /* can only go back to the beginning of the current buffer ... */
    abs_offset = (uint64_t) -offset;
    if (abs_offset > ipc->bytes) {
      fprintf (stderr, "ipcio_seek: %"PRIu64" > max backwards %"PRIu64"\n",
               abs_offset, ipc->bytes);
      return -1;
    }
    ipc->bytes -= abs_offset;
  }

  else {
    /* ... but can seek forward until end of data */

    while (offset && ! (ipcbuf_eod((ipcbuf_t*)ipc) && eobuf)) {

      if (!ipc->curbuf || eobuf) {
        ipc->curbuf = ipcbuf_get_next_read ((ipcbuf_t*)ipc, &(ipc->curbufsz));
        if (!ipc->curbuf) {
          fprintf (stderr, "ipcio_seek: error ipcbuf_next_read\n");
          return -1;
        }
        ipc->bytes = 0;
        eobuf = 0;
      }

      space = ipc->curbufsz - ipc->bytes;
      if (space > offset)
        space = offset;

      if (space > 0) {
        ipc->bytes += space;
        offset -= space;
      }
      else
        eobuf = 1;
    }

  }

  bufsz = ipcbuf_get_bufsz ((ipcbuf_t*)ipc);
  nbuf = ipcbuf_get_read_count ((ipcbuf_t*)ipc);
  if (nbuf > 0)
    nbuf -= 1;

  return bufsz * nbuf + ipc->bytes;
}

/* Returns the number of bytes available in the ring buffer */
int64_t ipcio_space_left(ipcio_t* ipc) {

  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t *)ipc);
  uint64_t nbufs = ipcbuf_get_nbufs ((ipcbuf_t *)ipc);
  uint64_t full_bufs = ipcbuf_get_nfull((ipcbuf_t*) ipc);
  uint64_t clear_bufs = ipcbuf_get_nclear((ipcbuf_t*) ipc);
  int64_t available_bufs = (nbufs - full_bufs);
/*
#ifdef _DEBUG
  fprintf(stderr,"full_bufs = %"PRIu64", clear_bufs = %"PRIu64", available_bufs = %"PRIu64", sum = %"PRIu64"\n",full_bufs, clear_bufs, available_bufs, available_bufs * bufsz);
#endif
*/
  return available_bufs * bufsz;

}

/* Returns the byte corresponding the start of data clocking/recording */
uint64_t ipcio_get_soclock_byte(ipcio_t* ipc) {
  uint64_t bufsz = ipcbuf_get_bufsz ((ipcbuf_t *)ipc);
  return bufsz * ((ipcbuf_t*)ipc)->soclock_buf;
}


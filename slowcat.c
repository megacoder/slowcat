/* slowcat, Copyright (c) 2000, 2001 Jamie Zawinski <jwz@dnalounge.com>
 *
 * Usage: slowcat bits-per-second byte-offset filename
 *
 *     Copy the given file to stdout, starting at byte-offset, and throttle
 *     the rate of output to be approximately (but not less than) the given
 *     bit rate.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * Created: 10-Jun-2000
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>

char *progname;
int verbose_p = 0;

static void
my_usleep (unsigned long usecs)
{
  struct timeval tv;
  tv.tv_sec  = usecs / 1000000L;
  tv.tv_usec = usecs % 1000000L;
  (void) select (0, 0, 0, 0, &tv);
}


static void
write_all (int fd, const char *buf, size_t count)
{
  while (count > 0)
    {
      int n = write (fd, buf, count);
      if (n < 0)
        {
          char buf2[1024];
          if (errno == EINTR || errno == EAGAIN)
            continue;

          sprintf (buf2, "%.255s: write:", progname);
          perror (buf2);
          exit (1);
        }

      count -= n;
      buf += n;
    }
}


static void
slowcat (int in_fd, int out_fd, int bps)
{
  static char buf[100 * 1024];
  int n;
  int total = 0;
  int batch_total = 0;
  time_t start = time ((time_t *) 0);
  time_t batch_start = start;
  time_t now = batch_start;
  int bufsiz = sizeof(buf)-1;

  bps /= 8;  /* bps is now bytes per second, not bits per second. */

  bufsiz = bps;

  while ((n = read (in_fd, buf, bufsiz)) != 0)
    {
      if (n < 0)
        {
          if (errno == EINTR || errno == EAGAIN)
            continue;

          sprintf (buf, "%.255s: read:", progname);
          perror (buf);
          exit (1);
        }

      batch_total += n;
      total += n;
      
      write_all (out_fd, buf, batch_total);

      if (verbose_p)
        fprintf (stderr, "%s: wrote %d bytes\n", progname, batch_total);

      if (batch_total >= bps)
        {
          int secs = batch_total / bps; /* how many seconds the batch we just
                                           wrote should have taken. */

          /* Wait for the second to tick. */
          while (now < batch_start + secs)
            {
              if (now < batch_start - 1)
                sleep (now - batch_start - 1);   /* N-1 seconds */
              else
                my_usleep (20000L);        /* 1/50th second */

              now = time ((time_t *) 0);
            }

          /* Second has ticked, restart counter and start writing again. */
          batch_start = now;
          batch_total = 0;
        }
    }

  if (verbose_p)
    {
      int secs, rate;
      double off;
      now = time ((time_t *) 0);
      secs = now - start;

      if (secs == 0)
        {
          fprintf (stderr, "%s: wrote %d bytes in one chunk.\n",
                   progname, total);
          return;
        }

      rate = total * 8 / secs;
      off = (double) rate / (double) (bps * 8);

      fprintf (stderr, "%s: actual bits per second: %d (%d bytes in %ds)\n",
               progname, rate, total, secs);

      if (off < 1.0)
        fprintf (stderr, "%s: undershot by %.1f%%\n", progname,
                 (1.0 - off) * 100);
      else if (off > 1.0)
        fprintf (stderr, "%s: overshot by %.1f%%\n", progname,
                 -(1.0 - off) * 100);
    }
}


int
main (int argc, char **argv)
{
  char *s, c;
  int bps, off, fd;
  char *file;
  struct stat st;
  progname = argv[0];
  s = strrchr (progname, '/');
  if (s) progname = s+1;

  if (argc > 1 && !strcmp(argv[1], "-v"))
    {
      verbose_p = 1;
      argc--;
      argv++;
    }
  
  if (argc != 4)
    {
    USAGE:
      fprintf (stderr, "usage: %s [-v] bits-per-second byte-offset filename\n",
               progname);
      exit (1);
    }

  if (1 != sscanf (argv[2], "%d%c", &off, &c)) goto USAGE;

  if (1 != sscanf (argv[1], "%d%c", &bps, &c))
    {
      if (1 != sscanf (argv[1], "%dk%c", &bps, &c) &&
          1 != sscanf (argv[1], "%dK%c", &bps, &c))
        goto USAGE;
      else
        bps *= 1024;
    }


  if (bps < 8 || bps > (1024 * 1024 * 1024))
    {
      fprintf (stderr, "%s: how about a sane bitrate?\n", progname);
      goto USAGE;
    }

  file = argv[3];
  fd = open (file, O_RDONLY);
  if (fd < 0)
    {
      char buf[1024];
      sprintf (buf, "%.255s: %.255s:", progname, file);
      perror (buf);
      exit (1);
    }

  if (0 > fstat(fd, &st))
    {
      char buf[1024];
      sprintf (buf, "%.255s: %.255s: fstat:", progname, file);
      perror (buf);
      exit (1);
    }

  if (off < 0 || off > st.st_size)
    {
      fprintf (stderr, "%s: byte-offset out of range 0-%ld\n",
               progname, (long) st.st_size);
      goto USAGE;
    }

  if (off != lseek (fd, off, SEEK_SET))
    {
      char buf[1024];
      sprintf (buf, "%.255s: %.255s: seek:", progname, file);
      perror (buf);
      exit (1);
    }

  slowcat (fd, fileno(stdout), bps);
  close (fd);
  return 0;
}

/**
 * Very simple logging.
 */

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>

#include "log.h"

// static long start_time = 0;

void log_msg(char *format, ...)
{
  va_list args;

  long ts;
  struct timeval tv;

  gettimeofday(&tv, NULL);

  ts = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

  char buf[4192];
  snprintf(buf, 4192, "%ld: %s", ts, format);

  va_start(args, format);
  vfprintf(stderr, buf, args);
  va_end(args);
}

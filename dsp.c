/*
 * dsp (Download SPeed) - A command line tool for testing internet
 * download speed.
 *
 * Copyright (C) 2013  Nathan Forbes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#define DSP_DEFAULT_PROGRAM_NAME "dsp"
#define DSP_VERSION              "1.0.0"

#define DSP_USER_AGENT \
  DSP_DEFAULT_PROGRAM_NAME " (Download SPeed tester)/" DSP_VERSION

#define DSP_ZERO_BYTES (0LU)

#define DSP_BINARY_K (1024LU)
#define DSP_BINARY_M (DSP_BINARY_K * DSP_BINARY_K)
#define DSP_BINARY_G (DSP_BINARY_K * DSP_BINARY_M)
#define DSP_BINARY_T (DSP_BINARY_K * DSP_BINARY_G)

#define DSP_METRIC_K (1000LU)
#define DSP_METRIC_M (DSP_METRIC_K * DSP_METRIC_K)
#define DSP_METRIC_G (DSP_METRIC_K * DSP_METRIC_M)
#define DSP_METRIC_T (DSP_METRIC_K * DSP_METRIC_G)

#define DSP_BYTE_SYMBOL "B"
#define DSP_BIT_SYMBOL  "bit"

#define DSP_BINARY_K_BYTE_SYMBOL "KiB"
#define DSP_BINARY_M_BYTE_SYMBOL "MiB"
#define DSP_BINARY_G_BYTE_SYMBOL "GiB"
#define DSP_BINARY_T_BYTE_SYMBOL "TiB"

#define DSP_BINARY_K_BIT_SYMBOL  "Kibit"
#define DSP_BINARY_M_BIT_SYMBOL  "Mibit"
#define DSP_BINARY_G_BIT_SYMBOL  "Gibit"
#define DSP_BINARY_T_BIT_SYMBOL  "Tibit"

#define DSP_METRIC_K_BYTE_SYMBOL "kB"
#define DSP_METRIC_M_BYTE_SYMBOL "MB"
#define DSP_METRIC_G_BYTE_SYMBOL "GB"
#define DSP_METRIC_T_BYTE_SYMBOL "TB"

#define DSP_METRIC_K_BIT_SYMBOL  "kbit"
#define DSP_METRIC_M_BIT_SYMBOL  "Mbit"
#define DSP_METRIC_G_BIT_SYMBOL  "Gbit"
#define DSP_METRIC_T_BIT_SYMBOL  "Tbit"

#define DSP_TIME_BUFFER_SIZE  64
#define DSP_SIZE_BUFFER_SIZE  16
#define DSP_SPEED_BUFFER_SIZE 20

#define DSP_SECONDS_IN_DAY        86400
#define DSP_SECONDS_IN_HOUR       3600
#define DSP_SECONDS_IN_MINUTE     60
#define DSP_MOD_VALUE_FOR_SECONDS 60

/* GNU's servers will probably be up for a long
   time and these files should also be there for while. */

/* about 13MB */
#define DSP_DEFAULT_URL_SMALL \
  "http://ftp.gnu.org/gnu/gcc/gcc-2.95.1.tar.gz"
  /* about 40MB */
#define DSP_DEFAULT_URL_MEDIUM \
    "http://ftp.gnu.org/gnu/gcc/gcc-4.7.0/gcc-4.6.3-4.7.0.diff.gz"
  /* about 80MB */
#define DSP_DEFAULT_URL_LARGE \
    "http://ftp.gnu.org/gnu/gcc/gcc-4.4.5/gcc-4.4.5.tar.gz"

#define DSP_FALSE ((dsp_boolean_t) 0)
#define DSP_TRUE  ((dsp_boolean_t) 1)

typedef unsigned char dsp_boolean_t;
typedef unsigned long dsp_byte_t;

const char *program_name;

dsp_boolean_t use_bit             = DSP_FALSE;
dsp_boolean_t use_byte            = DSP_FALSE;
dsp_boolean_t use_metric          = DSP_FALSE;
dsp_boolean_t use_binary          = DSP_FALSE;
dsp_boolean_t small_test          = DSP_FALSE;
dsp_boolean_t medium_test         = DSP_FALSE;
dsp_boolean_t large_test          = DSP_FALSE;
dsp_byte_t    total_bytes         = DSP_ZERO_BYTES;
dsp_byte_t    most_bytes_per_sec  = DSP_ZERO_BYTES;
dsp_byte_t    least_bytes_per_sec = DSP_ZERO_BYTES;
time_t        start_time          = ((time_t) 0);
time_t        end_time            = ((time_t) 0);
char *        user_supplied_url   = NULL;

static inline void
dsp_usage (dsp_boolean_t error)
{
  fprintf ((!error) ? stdout : stderr,
      "Usage: %s -[bBiLmMS] [-u URL]\n",
      program_name);
}

static inline void
dsp_help (void)
{
  dsp_usage (DSP_FALSE);
  fputs ("Options:\n"
      "  -b, --bit          Show result measurements in bits\n"
      "  -B, --byte         Show result measurements in bytes\n"
      "  -m, --metric       Use metric measurements (multiples of 1000)\n"
      "  -i, --binary       Use binary measurements (multiples of 1024)\n"
      "  -S, --small        Perform test using a small size download "
      "[about 12MB]\n"
      "                       (fastest default test but with the least\n"
      "                        accurate results)\n"
      "  -M, --medium       Perform test using a medium size download "
      "[about 40MB]\n"
      "                       (slower than the default small test but\n"
      "                        with slightly more accurate results)\n"
      "  -L, --large        Perform test using a large size download "
      "[about 80MB]\n"
      "                       (slowest default test but with the most\n"
      "                        accurate results)\n"
      "  -u URL, --url=URL  Perform test with URL instead of the default\n"
      "  -?, -h, --help     Show this message and exit\n"
      "  -v, --version      Show version information and exit\n"
      "Defaults:\n"
      "  If neither options `--bit' nor `--byte' are given, "
      "`--byte' is implied\n"
      "  If neither options `--metric' nor `--binary' are given, "
      "`--metric' is\n"
      "    implied\n"
      "  If none of the options `--small', `--medium' nor `--large' are "
      "given,\n"
      "    `--medium' is implied\n",
      stdout);
  exit (EXIT_SUCCESS);
}

static inline void
dsp_version (void)
{
  fputs (DSP_DEFAULT_PROGRAM_NAME " " DSP_VERSION "\n"
      "Copyright (C) 2013 Nathan Forbes\n"
      "License GPLv3+: GNU GPL version 3 or later "
        "<http://gnu.org/licenses/gpl.html>\n"
      "This is free software: you are free to change and redistribute it.\n"
      "There is NO WARRANTY, to the extent permitted by law.\n"
      "\n"
      "Written by Nathan Forbes\n",
      stdout);
  exit (EXIT_SUCCESS);
}

static void
dsp_perror (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: error: ", program_name);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
}

static inline dsp_boolean_t
dsp_streq (const char *s1, const char *s2)
{
  size_t n;

  n = strlen (s1);
  if ((n == strlen (s2)) && (memcmp (s1, s2, n) == 0))
    return DSP_TRUE;
  return DSP_FALSE;
}

static inline void
dsp_set_proper_program_name (char *argv0)
{
  char *x;

  if (argv0 && *argv0)
  {
    x = strrchr (argv0, '/');
    if (!x)
      program_name = argv0;
    else
      program_name = ++x;
    return;
  }
  program_name = DSP_DEFAULT_PROGRAM_NAME;
}

static inline void
dsp_parse_opts (char **v)
{
  size_t x;
  size_t y;

  dsp_set_proper_program_name (v[0]);

  if (!v[1])
  {
    use_byte = DSP_TRUE;
    use_metric = DSP_TRUE;
    medium_test = DSP_TRUE;
    return;
  }

  for (x = 1; v[x]; ++x)
  {
    if (dsp_streq (v[x], "-?") ||
        dsp_streq (v[x], "-h") ||
        dsp_streq (v[x], "--help"))
      dsp_help ();
    else if (dsp_streq (v[x], "-v") || dsp_streq (v[x], "--version"))
      dsp_version ();
    else if (dsp_streq (v[x], "-b") || dsp_streq (v[x], "--bit"))
      use_bit = DSP_TRUE;
    else if (dsp_streq (v[x], "-B") || dsp_streq (v[x], "--byte"))
      use_byte = DSP_TRUE;
    else if (dsp_streq (v[x], "-m") || dsp_streq (v[x], "--metric"))
      use_metric = DSP_TRUE;
    else if (dsp_streq (v[x], "-i") || dsp_streq (v[x], "--binary"))
      use_binary = DSP_TRUE;
    else if (dsp_streq (v[x], "-S") || dsp_streq (v[x], "--small"))
      small_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-M") || dsp_streq (v[x], "--medium"))
      medium_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-L") || dsp_streq (v[x], "--large"))
      large_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-u") || dsp_streq (v[x], "--url"))
    {
      if (!v[x + 1] || v[x + 1][0] == '-')
      {
        dsp_perror ("`%s' requires an argument", v[x]);
        dsp_usage (DSP_TRUE);
        exit (EXIT_FAILURE);
      }
      else
      {
        user_supplied_url = v[++x];
        continue;
      }
    }
    else if (strncmp (v[x], "--url=", 6) == 0)
    {
      user_supplied_url = strchr (v[x], '=');
      if (user_supplied_url)
        ++user_supplied_url;
      if (!user_supplied_url || !*user_supplied_url)
      {
        dsp_perror ("`--url' requires an argument");
        dsp_usage (DSP_TRUE);
        exit (EXIT_FAILURE);
      }
    }
    else
    {
      if (v[x][0] == '-')
      {
        for (y = 1; v[x][y]; ++y)
        {
          switch (v[x][y])
          {
            case 'b':
              use_bit = DSP_TRUE;
              break;
            case 'B':
              use_byte = DSP_TRUE;
              break;
            case 'i':
              use_binary = DSP_TRUE;
              break;
            case 'm':
              use_metric = DSP_TRUE;
              break;
            case 'S':
              small_test = DSP_TRUE;
              break;
            case 'M':
              medium_test = DSP_TRUE;
              break;
            case 'L':
              large_test = DSP_TRUE;
              break;
            case 'u':
              dsp_perror ("`-u' requires an argument");
              dsp_usage (DSP_TRUE);
              exit (EXIT_FAILURE);
            default:
              dsp_perror ("unrecognized option `-%c'", v[x][y]);
              dsp_usage (DSP_TRUE);
              exit (EXIT_FAILURE);
          }
        }
      }
      else
      {
        dsp_perror ("unrecognized option `%s'", v[x]);
        dsp_usage (DSP_TRUE);
        exit (EXIT_FAILURE);
      }
    }
  }

  if (use_bit && use_byte)
  {
    dsp_perror ("`-b'/`--bit' and `-B'/`--byte' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (use_metric && use_binary)
  {
    dsp_perror ("`-m'/`--metric' and `-i'/`--binary' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (small_test && medium_test && large_test)
  {
    dsp_perror ("`-S'/`--small', `-M'/`--medium' and `-L'/`--large' "
        "are all mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (small_test)
  {
    if (medium_test || large_test)
    {
      dsp_perror ("`-S'/`--small' and `-%c'/`--%s' are mutually exclusive",
          (medium_test) ? 'M' : 'L', (medium_test) ? "medium" : "large");
      exit (EXIT_FAILURE);
    }
  }

  if (medium_test && large_test)
  {
    dsp_perror ("`-M'/`--medium' and `-L'/`--large' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (user_supplied_url && *user_supplied_url)
  {
    if (small_test || medium_test || large_test)
    {
      fprintf (stderr, "%s: error: `-%c'/`--%s' and `-u'/`--url' "
          "are mutually exclusive\n", program_name,
          (small_test) ? 'S' : (medium_test) ? 'M' : 'L',
          (small_test) ? "small" : (medium_test) ? "medium" : "large");
      exit (EXIT_FAILURE);
    }
  }

  if (!small_test && !medium_test && !large_test)
    medium_test = DSP_TRUE;

  if (!use_bit && !use_byte)
    use_byte = DSP_TRUE;

  if (!use_metric && !use_binary)
    use_metric = DSP_TRUE;
}

static inline void
dsp_format_size (char *buf, size_t n, dsp_byte_t bytes)
{
  dsp_byte_t bits;

  buf[0] = '\0';
  if (use_byte &&
      ((use_metric && bytes < DSP_METRIC_K) ||
       (use_binary && bytes < DSP_BINARY_K)))
  {
    snprintf (buf, n, "%lu " DSP_BYTE_SYMBOL, bytes);
    return;
  }

  bits = DSP_ZERO_BYTES;
  if (use_bit)
  {
    bits = (bytes * ((dsp_byte_t) CHAR_BIT));
    if ((use_metric && bits < DSP_METRIC_K) ||
        (use_binary && bits < DSP_BINARY_K))
    {
      snprintf (buf, n, "%lu " DSP_BIT_SYMBOL, bits);
      return;
    }
  }

  if (use_byte)
  {
#define __DSP_SFMT(__s, __v) \
  snprintf (buf, n, "%.2f " __s, (((double) bytes) / ((double) __v)))
    if (use_metric)
    {
      if ((bytes / DSP_METRIC_T) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_T_BYTE_SYMBOL, DSP_METRIC_T);
      else if ((bytes / DSP_METRIC_G) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_G_BYTE_SYMBOL, DSP_METRIC_G);
      else if ((bytes / DSP_METRIC_M) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_M_BYTE_SYMBOL, DSP_METRIC_M);
      else if ((bytes / DSP_METRIC_K) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_K_BYTE_SYMBOL, DSP_METRIC_K);
    }
    else
    {
      if ((bytes / DSP_BINARY_T) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_T_BYTE_SYMBOL, DSP_BINARY_T);
      else if ((bytes / DSP_BINARY_G) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_G_BYTE_SYMBOL, DSP_BINARY_G);
      else if ((bytes / DSP_BINARY_M) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_M_BYTE_SYMBOL, DSP_BINARY_M);
      else if ((bytes / DSP_BINARY_K) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_K_BYTE_SYMBOL, DSP_BINARY_K);
    }
  }
  else
  {
#undef __DSP_SFMT
#define __DSP_SFMT(__s, __v) snprintf (buf, n, "%lu " __s, (bits / __v))
    if (bits == DSP_ZERO_BYTES)
      return;
    if (use_metric)
    {
      if ((bits / DSP_METRIC_T) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_T_BIT_SYMBOL, DSP_METRIC_T);
      else if ((bits / DSP_METRIC_G) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_G_BIT_SYMBOL, DSP_METRIC_G);
      else if ((bits / DSP_METRIC_M) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_M_BIT_SYMBOL, DSP_METRIC_M);
      else if ((bits / DSP_METRIC_K) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_K_BIT_SYMBOL, DSP_METRIC_K);
    }
    else
    {
      if ((bits / DSP_BINARY_T) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_T_BIT_SYMBOL, DSP_BINARY_T);
      else if ((bits / DSP_BINARY_G) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_G_BIT_SYMBOL, DSP_BINARY_G);
      else if ((bits / DSP_BINARY_M) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_M_BIT_SYMBOL, DSP_BINARY_M);
      else if ((bits / DSP_BINARY_K) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_K_BIT_SYMBOL, DSP_BINARY_K);
    }
  }
}

static inline void
dsp_format_rate (char *buf, size_t n, dsp_boolean_t avg, dsp_byte_t bytes)
{
  int elapsed;
  size_t x;
  dsp_byte_t t_bytes;

  if (avg)
  {
    elapsed = (((int) start_time) - ((int) end_time));
    if (elapsed < 0)
      elapsed = -elapsed;
    t_bytes = (bytes / ((dsp_byte_t) elapsed));
    dsp_format_size (buf, DSP_SIZE_BUFFER_SIZE, t_bytes);
  }
  else
    dsp_format_size (buf, DSP_SIZE_BUFFER_SIZE, bytes);

  if (*buf)
  {
    x = strlen (buf);
    buf[x++] = '/';
    buf[x++] = 's';
    buf[x] = '\0';
  }
}

static inline void
dsp_format_time (char *buf, size_t n)
{
  int elapsed;
  int days; /* god forbid... */
  int hours;
  int minutes;
  int seconds;
  size_t x;

  elapsed = (((int) start_time) - ((int) end_time));
  if (elapsed < 0)
    elapsed = -elapsed;

  days = (elapsed / DSP_SECONDS_IN_DAY);
  hours = (elapsed / DSP_SECONDS_IN_HOUR);
  minutes = (elapsed / DSP_SECONDS_IN_MINUTE);
  seconds = (elapsed % DSP_MOD_VALUE_FOR_SECONDS);
  x = 0;

  if (days > 0)
  {
    snprintf (buf, n, "%i day%s", days, (days == 1) ? "" : "s");
    x = strlen (buf);
    if ((hours == 0) && (minutes == 0) && (seconds == 0))
    {
      snprintf (buf + x, n - x, " exactly");
      return;
    }
  }

  if (hours > 0)
  {
    snprintf (buf, n, "%s%i hour%s",
        (days > 0) ? " " : "", hours, (hours == 1) ? "" : "s");
    x = strlen (buf);
    if ((minutes == 0) && (seconds == 0))
    {
      snprintf (buf + x, n - x, " exactly");
      return;
    }
  }

  if (minutes > 0)
  {
    snprintf (buf + x, n - x, "%s%i minute%s",
        ((days > 0) || (hours > 0)) ? " " : "",
        minutes, (minutes == 1) ? "" : "s");
    x = strlen (buf);
    if (seconds == 0)
    {
      snprintf (buf + x, n - x, " exactly");
      return;
    }
  }

  if (seconds > 0)
    snprintf (buf + x, n - x, "%s%i second%s",
        ((days > 0) || (hours > 0) || (minutes > 0)) ? " " : "",
        seconds, (seconds == 1) ? "" : "s");
}

static inline dsp_boolean_t
dsp_nan_value (double v)
{
  volatile double x;

  x = v;
  if (x != x)
    return DSP_TRUE;
  return DSP_FALSE;
}

static inline int
dsp_download_progress (void *data,
                       double d_total,
                       double d_current,
                       double u_total,
                       double u_current)
{
  static time_t last_time  = ((time_t) -1);
  static dsp_byte_t last_bytes = DSP_ZERO_BYTES;

  double x;
  double elapsed;
  time_t now_time;
  dsp_byte_t now_bytes;
  dsp_byte_t bytes_this_sec;

  if (!total_bytes && (d_total > 0.0))
    total_bytes = ((dsp_byte_t) d_total);

  if (last_time)
  {
    now_time = time (NULL);
    elapsed = (((double) now_time) - ((double) last_time));
    if (elapsed >= 1)
    {
      now_bytes = ((dsp_byte_t) d_current);
      if (last_bytes == DSP_ZERO_BYTES)
        last_bytes = now_bytes;
      else if (now_bytes > last_bytes)
      {
        bytes_this_sec = (now_bytes - last_bytes);
        if (bytes_this_sec)
        {
          if (bytes_this_sec > most_bytes_per_sec)
            most_bytes_per_sec = bytes_this_sec;
          if ((least_bytes_per_sec == DSP_ZERO_BYTES) ||
              (bytes_this_sec < least_bytes_per_sec))
            least_bytes_per_sec = bytes_this_sec;
        }
      }
      last_time = now_time;
      last_bytes = now_bytes;
    }
  }
  else
    last_time = time (NULL);

  fputs ("Calculating... (", stdout);
  x = (d_current / d_total);

  if (dsp_nan_value (x) || dsp_nan_value ((x * 100)))
    fputs ("0%", stdout);
  else
    fprintf (stdout, "%.0f%%", (x * 100));

  fputs (")\r", stdout);
  fflush (stdout);
  return 0;
}

static inline dsp_boolean_t
dsp_setup_curl (CURL **cp, FILE **fp)
{
  int s_errno;
  CURLcode c_status;
  const char *url;

  *fp = NULL;
  c_status = CURLE_OK;

  *cp = curl_easy_init ();
  if (!*cp)
  {
    c_status = CURLE_FAILED_INIT;
    goto failure_with_curl;
  }

  if (user_supplied_url && *user_supplied_url)
    url = user_supplied_url;
  else
  {
    if (small_test)
      url = DSP_DEFAULT_URL_SMALL;
    else if (medium_test)
      url = DSP_DEFAULT_URL_MEDIUM;
    else if (large_test)
      url = DSP_DEFAULT_URL_LARGE;
    else
      url = NULL;
  }

  c_status = curl_easy_setopt (*cp, CURLOPT_URL, url);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  c_status = curl_easy_setopt (*cp, CURLOPT_USERAGENT, DSP_USER_AGENT);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  c_status = curl_easy_setopt (*cp, CURLOPT_FOLLOWLOCATION, 1L);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  /* treat downloaded file as a temporary file
     since we don't actually care about its contents */
  *fp = tmpfile ();
  if (!*fp)
    goto failure;

  c_status = curl_easy_setopt (*cp, CURLOPT_WRITEDATA, (void *) *fp);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  c_status = curl_easy_setopt (*cp, CURLOPT_WRITEFUNCTION, (void *) fwrite);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  c_status = curl_easy_setopt (*cp, CURLOPT_NOPROGRESS, 0L);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  c_status =
    curl_easy_setopt (*cp, CURLOPT_PROGRESSFUNCTION,
        (void *) dsp_download_progress);
  if (c_status != CURLE_OK)
    goto failure_with_curl;

  return DSP_TRUE;

failure:
  s_errno = errno;
  fprintf (stderr, "%s: error: %s\n",
      program_name, strerror (s_errno));
  if (*fp)
    fclose (*fp);
  if (*cp)
    curl_easy_cleanup (*cp);
  return DSP_FALSE;

failure_with_curl:
  fprintf (stderr, "%s: error: %s\n",
      program_name, curl_easy_strerror (c_status));
  if (*fp)
    fclose (*fp);
  if (*cp)
    curl_easy_cleanup (*cp);
  return DSP_FALSE;
}

static inline void
dsp_perform (void)
{
  FILE *fp;
  CURL *cp;
  CURLcode c_status;
  char average_rate[DSP_SPEED_BUFFER_SIZE];
  char fastest_rate[DSP_SPEED_BUFFER_SIZE];
  char slowest_rate[DSP_SPEED_BUFFER_SIZE];
  char total_down[DSP_SIZE_BUFFER_SIZE];
  char total_time[DSP_TIME_BUFFER_SIZE];

  if (!dsp_setup_curl (&cp, &fp))
    exit (EXIT_FAILURE);

  start_time = time (NULL);
  c_status = curl_easy_perform (cp);
  end_time = time (NULL);

  if (fp)
    fclose (fp);

  if (c_status != CURLE_OK)
  {
    fprintf (stderr, "%s: error: %s\n",
        program_name, curl_easy_strerror (c_status));
    curl_easy_cleanup (cp);
    exit (EXIT_FAILURE);
  }

  curl_easy_cleanup (cp);

  dsp_format_size (total_down, DSP_SIZE_BUFFER_SIZE, total_bytes);
  dsp_format_time (total_time, DSP_TIME_BUFFER_SIZE);
  dsp_format_rate (average_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_TRUE, total_bytes);
  dsp_format_rate (fastest_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_FALSE, most_bytes_per_sec);
  dsp_format_rate (slowest_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_FALSE, least_bytes_per_sec);

  fprintf (stdout,
      "Total time:       %s\n"
      "Total d/l:        %s\n"
      "Average d/l rate: %s\n"
      "Peak d/l rate:    %s\n"
      "Lowest d/l rate:  %s\n",
      total_time,
      total_down,
      average_rate,
      fastest_rate,
      slowest_rate);
}

int
main (int argc, char **argv)
{
  dsp_parse_opts (argv);
  dsp_perform ();
  exit (EXIT_SUCCESS);
  return 0; /* for compilers */
}


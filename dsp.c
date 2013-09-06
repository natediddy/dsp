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

#include "dsp.h"

const char *program_name;

dsp_boolean_t use_bit = DSP_FALSE;
dsp_boolean_t use_byte = DSP_FALSE;
dsp_boolean_t use_metric = DSP_FALSE;
dsp_boolean_t use_binary = DSP_FALSE;
dsp_boolean_t small_test = DSP_FALSE;
dsp_boolean_t medium_test = DSP_FALSE;
dsp_boolean_t large_test = DSP_FALSE;

dsp_byte_t total_bytes = DSP_ZERO_BYTES;
dsp_byte_t most_bytes_per_sec = DSP_ZERO_BYTES;
dsp_byte_t least_bytes_per_sec = DSP_ZERO_BYTES;

time_t start_time = -1;
time_t end_time = -1;

char *user_supplied_url = NULL;

static void
dsp_usage (dsp_boolean_t error)
{
  fprintf ((!error) ? stdout : stderr,
      "Usage: %s -[" DSP_OPTION_SHORT_BIT_S DSP_OPTION_SHORT_BYTE_S
      DSP_OPTION_SHORT_BINARY_S DSP_OPTION_SHORT_LARGE_S
      DSP_OPTION_SHORT_METRIC_S DSP_OPTION_SHORT_MEDIUM_S
      DSP_OPTION_SHORT_SMALL_S "] [--" DSP_OPTION_LONG_URL "=URL]\n",
      program_name);
}

static void
dsp_help (void)
{
  dsp_usage (DSP_FALSE);
  fputs ("Options:\n"

      "  -" DSP_OPTION_SHORT_BIT_S ", --" DSP_OPTION_LONG_BIT
      "          Show result measurements in bits\n"

      "  -" DSP_OPTION_SHORT_BYTE_S ", --" DSP_OPTION_LONG_BYTE
      "         Show result measurements in bytes\n"

      "  -" DSP_OPTION_SHORT_METRIC_S ", --" DSP_OPTION_LONG_METRIC
      "       Use metric measurements (multiples of 1000)\n"

      "  -" DSP_OPTION_SHORT_BINARY_S ", --" DSP_OPTION_LONG_BINARY
      "       Use binary measurements (multiples of 1024)\n"

      "  -" DSP_OPTION_SHORT_SMALL_S ", --" DSP_OPTION_LONG_SMALL
      "        Perform test using a small size download [about 13MB]\n"
      "                       (fastest default test but with the least\n"
      "                        accurate results)\n"

      "  -" DSP_OPTION_SHORT_MEDIUM_S ", --" DSP_OPTION_LONG_MEDIUM
      "       Perform test using a medium size download [about 40MB]\n"
      "                       (slower than the default small test but\n"
      "                        with slightly more accurate results)\n"

      "  -" DSP_OPTION_SHORT_LARGE_S ", --" DSP_OPTION_LONG_LARGE
      "        Perform test using a large size download [about 82MB]\n"
      "                       (slowest default test but with the most\n"
      "                        accurate results)\n"

      "  -" DSP_OPTION_SHORT_URL_S " URL, --" DSP_OPTION_LONG_URL
      "=URL  Perform test with URL instead of the default\n"

      "  -" DSP_OPTION_SHORT_HELP0_S ", -" DSP_OPTION_SHORT_HELP1_S ", --"
      DSP_OPTION_LONG_HELP "     Show this message and exit\n"

      "  -" DSP_OPTION_SHORT_VERSION_S ", --" DSP_OPTION_LONG_VERSION
      "      Show version information and exit\n"

      "Defaults:\n"

      "  If neither options `--" DSP_OPTION_LONG_BIT "' nor `--"
      DSP_OPTION_LONG_BYTE "' are given, `--" DSP_OPTION_LONG_BYTE
      "' is implied\n"

      "  If neither options `--" DSP_OPTION_LONG_METRIC "' nor `--"
      DSP_OPTION_LONG_BINARY "' are given, `--" DSP_OPTION_LONG_METRIC
      "' is\n"
      "    implied\n"

      "  If none of the options `--" DSP_OPTION_LONG_SMALL "', `--"
      DSP_OPTION_LONG_MEDIUM "' nor `--" DSP_OPTION_LONG_LARGE
      "' are given,\n"
      "    `--" DSP_OPTION_LONG_MEDIUM "' is implied\n",
      stdout);
  exit (EXIT_SUCCESS);
}

static void
dsp_version (void)
{
  fputs (DSP_DEFAULT_PROGRAM_NAME " " DSP_VERSION "\n"
      "Copyright (C) 2013 Nathan Forbes <me@nathanforbes.com>\n"
      "License GPLv3+: GNU GPL version 3 or later "
        "<http://gnu.org/licenses/gpl.html>\n"
      "This is free software: you are free to change and redistribute it.\n"
      "There is NO WARRANTY, to the extent permitted by law.\n"
      "\n"
      "Written by Nathan Forbes.\n",
      stdout);
  exit (EXIT_SUCCESS);
}

static void
dsp_perror (const char *str, ...)
{
  size_t x;
  va_list args;
  dsp_boolean_t fmt;

  fputs (program_name, stderr);
  fputs (": error: ", stderr);

  fmt = DSP_FALSE;
  for (x = 0; str[x]; ++x)
  {
    if (str[x] == '%')
    {
      fmt = DSP_TRUE;
      break;
    }
  }

  if (!fmt)
    fputs (str, stderr);
  else
  {
    va_start (args, str);
    vfprintf (stderr, str, args);
    va_end (args);
  }
  fputc ('\n', stderr);
}

static dsp_boolean_t
dsp_streq (const char *s1, const char *s2)
{
  size_t n;

  n = strlen (s1);
  if ((n == strlen (s2)) && (memcmp (s1, s2, n) == 0))
    return DSP_TRUE;
  return DSP_FALSE;
}

static void
dsp_set_proper_program_name (char *v)
{
  char *x;

  if (v && *v)
  {
    x = strrchr (v, '/');
    if (!x)
      program_name = v;
    else
      program_name = ++x;
    return;
  }
  program_name = DSP_DEFAULT_PROGRAM_NAME;
}

static void
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
    if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_HELP0_S) ||
        dsp_streq (v[x], "-" DSP_OPTION_SHORT_HELP1_S) ||
        dsp_streq (v[x], "--" DSP_OPTION_LONG_HELP))
      dsp_help ();
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_VERSION_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_VERSION))
      dsp_version ();
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_BIT_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_BIT))
      use_bit = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_BYTE_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_BYTE))
      use_byte = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_METRIC_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_METRIC))
      use_metric = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_BINARY_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_BINARY))
      use_binary = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_SMALL_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_SMALL))
      small_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_MEDIUM_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_MEDIUM))
      medium_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_LARGE_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_LARGE))
      large_test = DSP_TRUE;
    else if (dsp_streq (v[x], "-" DSP_OPTION_SHORT_URL_S) ||
             dsp_streq (v[x], "--" DSP_OPTION_LONG_URL))
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
    else if (strncmp (v[x], "--" DSP_OPTION_LONG_URL "=",
                      strlen ("--" DSP_OPTION_LONG_URL "=")) == 0)
    {
      user_supplied_url = strchr (v[x], '=');
      if (user_supplied_url)
        ++user_supplied_url;
      if (!user_supplied_url || !*user_supplied_url)
      {
        dsp_perror ("`--" DSP_OPTION_LONG_URL "' requires an argument");
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
            case DSP_OPTION_SHORT_BIT_C:
              use_bit = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_BYTE_C:
              use_byte = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_BINARY_C:
              use_binary = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_METRIC_C:
              use_metric = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_SMALL_C:
              small_test = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_MEDIUM_C:
              medium_test = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_LARGE_C:
              large_test = DSP_TRUE;
              break;
            case DSP_OPTION_SHORT_URL_C:
              dsp_perror ("`-" DSP_OPTION_SHORT_URL_S "' requires an argument");
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
    dsp_perror ("`-" DSP_OPTION_SHORT_BIT_S "'/`--" DSP_OPTION_LONG_BIT
        "' and `-" DSP_OPTION_SHORT_BYTE_S "'/`--" DSP_OPTION_LONG_BYTE
        "' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (use_metric && use_binary)
  {
    dsp_perror ("`-" DSP_OPTION_SHORT_METRIC_S "'/`--" DSP_OPTION_LONG_METRIC
        "' and `-" DSP_OPTION_SHORT_BINARY_S "'/`--" DSP_OPTION_LONG_BINARY
        "' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (small_test && medium_test && large_test)
  {
    dsp_perror ("`-" DSP_OPTION_SHORT_SMALL_S "'/`--" DSP_OPTION_LONG_SMALL
        "', `-" DSP_OPTION_SHORT_MEDIUM_S "'/`--" DSP_OPTION_LONG_MEDIUM
        "' and `-" DSP_OPTION_SHORT_LARGE_S "'/`--" DSP_OPTION_LONG_LARGE
        "' are all mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (small_test)
  {
    if (medium_test || large_test)
    {
      dsp_perror ("`-" DSP_OPTION_SHORT_SMALL_S "'/`--" DSP_OPTION_LONG_SMALL
          "' and `-%c'/`--%s' are mutually exclusive",
          (medium_test) ? DSP_OPTION_SHORT_MEDIUM_C :
          DSP_OPTION_SHORT_LARGE_C,
          (medium_test) ? DSP_OPTION_LONG_MEDIUM : DSP_OPTION_LONG_LARGE);
      exit (EXIT_FAILURE);
    }
  }

  if (medium_test && large_test)
  {
    dsp_perror ("`-" DSP_OPTION_SHORT_MEDIUM_S "'/`--" DSP_OPTION_LONG_MEDIUM
        "' and `-" DSP_OPTION_SHORT_LARGE_S "'/`--" DSP_OPTION_LONG_LARGE
        "' are mutually exclusive");
    exit (EXIT_FAILURE);
  }

  if (user_supplied_url && *user_supplied_url)
  {
    if (small_test || medium_test || large_test)
    {
      dsp_perror ("%`-%c'/`--%s' and `-" DSP_OPTION_SHORT_URL_S "'/`--"
          DSP_OPTION_LONG_URL "' are mutually exclusive",
          (small_test) ? DSP_OPTION_SHORT_SMALL_C : (medium_test) ?
              DSP_OPTION_SHORT_MEDIUM_C : DSP_OPTION_SHORT_LARGE_C,
          (small_test) ? DSP_OPTION_LONG_SMALL : (medium_test) ?
              DSP_OPTION_LONG_MEDIUM : DSP_OPTION_LONG_LARGE);
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

static void
dsp_format_size (char *buf, size_t n, dsp_byte_t bytes)
{
  dsp_byte_t bits;

  buf[0] = '\0';
  if (use_byte &&
      ((use_metric && bytes < DSP_METRIC_KILO) ||
       (use_binary && bytes < DSP_BINARY_KIBI)))
  {
    snprintf (buf, n, "%lu " DSP_BYTE_SYMBOL, bytes);
    return;
  }

  bits = DSP_ZERO_BYTES;
  if (use_bit)
  {
    bits = (bytes * ((dsp_byte_t) CHAR_BIT));
    if ((use_metric && bits < DSP_METRIC_KILO) ||
        (use_binary && bits < DSP_BINARY_KIBI))
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
      if ((bytes / DSP_METRIC_TERA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_TERABYTE_SYMBOL, DSP_METRIC_TERA);
      else if ((bytes / DSP_METRIC_GIGA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_GIGABYTE_SYMBOL, DSP_METRIC_GIGA);
      else if ((bytes / DSP_METRIC_MEGA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_MEGABYTE_SYMBOL, DSP_METRIC_MEGA);
      else if ((bytes / DSP_METRIC_KILO) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_KILOBYTE_SYMBOL, DSP_METRIC_KILO);
    }
    else
    {
      if ((bytes / DSP_BINARY_TEBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_TEBIBYTE_SYMBOL, DSP_BINARY_TEBI);
      else if ((bytes / DSP_BINARY_GIBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_GIBIBYTE_SYMBOL, DSP_BINARY_GIBI);
      else if ((bytes / DSP_BINARY_MEBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_MEBIBYTE_SYMBOL, DSP_BINARY_MEBI);
      else if ((bytes / DSP_BINARY_KIBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_KIBIBYTE_SYMBOL, DSP_BINARY_KIBI);
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
      if ((bits / DSP_METRIC_TERA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_TERABIT_SYMBOL, DSP_METRIC_TERA);
      else if ((bits / DSP_METRIC_GIGA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_GIGABIT_SYMBOL, DSP_METRIC_GIGA);
      else if ((bits / DSP_METRIC_MEGA) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_MEGABIT_SYMBOL, DSP_METRIC_MEGA);
      else if ((bits / DSP_METRIC_KILO) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_METRIC_KILOBIT_SYMBOL, DSP_METRIC_KILO);
    }
    else
    {
      if ((bits / DSP_BINARY_TEBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_TEBIBIT_SYMBOL, DSP_BINARY_TEBI);
      else if ((bits / DSP_BINARY_GIBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_GIBIBIT_SYMBOL, DSP_BINARY_GIBI);
      else if ((bits / DSP_BINARY_MEBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_MEBIBIT_SYMBOL, DSP_BINARY_MEBI);
      else if ((bits / DSP_BINARY_KIBI) > DSP_ZERO_BYTES)
        __DSP_SFMT (DSP_BINARY_KIBIBIT_SYMBOL, DSP_BINARY_KIBI);
    }
  }
}

static void
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

static void
dsp_format_time (char *buf, size_t n)
{
  int elapsed;
  int days;
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

static dsp_boolean_t
dsp_nan_value (double v)
{
  volatile double x;

  x = v;
  if (x != x)
    return DSP_TRUE;
  return DSP_FALSE;
}

static int
dsp_download_progress (void *data,
                       double d_total,
                       double d_current,
                       double u_total,
                       double u_current)
{
  static time_t last_time = -1;
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

static dsp_boolean_t
dsp_setup_curl (CURL **cp, FILE **fp)
{
  int s_errno;
  CURLcode c_status;
  const char *url;

  c_status = CURLE_OK;
  *cp = curl_easy_init ();
  if (!*cp)
  {
    c_status = CURLE_FAILED_INIT;
    goto failure;
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
    goto failure;

  c_status = curl_easy_setopt (*cp, CURLOPT_USERAGENT, DSP_USER_AGENT);
  if (c_status != CURLE_OK)
    goto failure;

  c_status = curl_easy_setopt (*cp, CURLOPT_FOLLOWLOCATION, 1L);
  if (c_status != CURLE_OK)
    goto failure;

  /* treat the downloaded file as a temporary file
     since we don't actually care about its contents */
  *fp = tmpfile ();
  if (!*fp)
  {
    s_errno = errno;
    dsp_perror (strerror (s_errno));
    curl_easy_cleanup (*cp);
    return DSP_FALSE;
  }

  c_status = curl_easy_setopt (*cp, CURLOPT_WRITEDATA, (void *) *fp);
  if (c_status != CURLE_OK)
    goto failure;

  c_status = curl_easy_setopt (*cp, CURLOPT_WRITEFUNCTION, (void *) fwrite);
  if (c_status != CURLE_OK)
    goto failure;

  c_status = curl_easy_setopt (*cp, CURLOPT_NOPROGRESS, 0L);
  if (c_status != CURLE_OK)
    goto failure;

  c_status =
    curl_easy_setopt (*cp, CURLOPT_PROGRESSFUNCTION,
        (void *) dsp_download_progress);
  if (c_status != CURLE_OK)
    goto failure;

  return DSP_TRUE;

failure:
  dsp_perror (curl_easy_strerror (c_status));
  if (*fp)
    fclose (*fp);
  if (*cp)
    curl_easy_cleanup (*cp);
  return DSP_FALSE;
}

static void
dsp_fill_display_data (dsp_display_data_t *dd)
{
  memset (dd, 0, sizeof (dsp_display_data_t));

  dd->total_down_size[0] = '\0';
  dd->total_down_time[0] = '\0';
  dd->average_down_rate[0] = '\0';
  dd->peak_down_rate[0] = '\0';
  dd->lowest_down_rate[0] = '\0';

  dsp_format_size (dd->total_down_size, DSP_SIZE_BUFFER_SIZE, total_bytes);
  dsp_format_time (dd->total_down_time, DSP_TIME_BUFFER_SIZE);
  dsp_format_rate (dd->average_down_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_TRUE, total_bytes);
  dsp_format_rate (dd->peak_down_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_FALSE, most_bytes_per_sec);
  dsp_format_rate (dd->lowest_down_rate,
      DSP_SPEED_BUFFER_SIZE, DSP_FALSE, least_bytes_per_sec);
}

static void
dsp_show_display_data (dsp_display_data_t *dd)
{
  fputs (DSP_TOTAL_DOWN_TIME_DISPLAY_TAG, stdout);
  if (*dd->total_down_time)
    fputs (dd->total_down_time, stdout);
  else
    fputs (DSP_UNKNOWN_DISPLAY_DATA, stdout);
  fputc ('\n', stdout);

  fputs (DSP_TOTAL_DOWN_SIZE_DISPLAY_TAG, stdout);
  if (*dd->total_down_size)
    fputs (dd->total_down_size, stdout);
  else
    fputs (DSP_UNKNOWN_DISPLAY_DATA, stdout);
  fputc ('\n', stdout);

  fputs (DSP_AVERAGE_DOWN_RATE_DISPLAY_TAG, stdout);
  if (*dd->average_down_rate)
    fputs (dd->average_down_rate, stdout);
  else
    fputs (DSP_UNKNOWN_DISPLAY_DATA, stdout);
  fputc ('\n', stdout);

  fputs (DSP_PEAK_DOWN_RATE_DISPLAY_TAG, stdout);
  if (*dd->peak_down_rate)
    fputs (dd->peak_down_rate, stdout);
  else
    fputs (DSP_UNKNOWN_DISPLAY_DATA, stdout);
  fputc ('\n', stdout);

  fputs (DSP_LOWEST_DOWN_RATE_DISPLAY_TAG, stdout);
  if (*dd->lowest_down_rate)
    fputs (dd->lowest_down_rate, stdout);
  else
    fputs (DSP_UNKNOWN_DISPLAY_DATA, stdout);
  fputc ('\n', stdout);
}

static void
dsp_perform (void)
{
  FILE *fp;
  CURL *cp;
  CURLcode c_status;
  dsp_display_data_t dd;

  fp = NULL;
  cp = NULL;

  if (!dsp_setup_curl (&cp, &fp))
    exit (EXIT_FAILURE);

  start_time = time (NULL);
  c_status = curl_easy_perform (cp);
  end_time = time (NULL);

  if (fp)
    fclose (fp);

  if (c_status != CURLE_OK)
  {
    dsp_perror (curl_easy_strerror (c_status));
    curl_easy_cleanup (cp);
    exit (EXIT_FAILURE);
  }

  curl_easy_cleanup (cp);
  dsp_fill_display_data (&dd);
  dsp_show_display_data (&dd);
}

int
main (int argc, char **argv)
{
  dsp_parse_opts (argv);
  dsp_perform ();
  exit (EXIT_SUCCESS);
  return 0; /* for compilers */
}


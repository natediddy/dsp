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
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
# include <windows.h>
#else
# include <sys/ioctl.h>
# include <unistd.h>
#endif

#include <curl/curl.h>

#include "dsp.h"

const char *program_name;

char *user_supplied_url = NULL;
char *temp_file_path = NULL;

time_t start_time = -1;
time_t end_time = -1;

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

dsp_unit_option_t unit_option = DSP_UNIT_OPTION_0;

static void
dsp_usage (dsp_boolean_t error)
{
  FILE *out_stream;

  if (!error)
    out_stream = stdout;
  else
    out_stream = stderr;

  fputs ("Usage: ", out_stream);
  fputs (program_name, out_stream);
  fputs (": -[bBiLmMS] [--unit=UNIT] [--url=URL]\n", out_stream);
}

static void
dsp_help (void)
{
  dsp_usage (DSP_FALSE);
  fputs ("Options:\n"
      "  -b, --bit              Show result measurements in bits\n"
      "  -B, --byte             Show result measurements in bytes\n"
      "  -m, --metric           Use metric measurements (multiples of 1000)\n"
      "  -i, --binary           Use binary measurements (multiples of 1024)\n"
      "  -n UNIT, --unit=UNIT   Measure results stricty in UNIT\n"
      "                           UNIT can be any of the following:\n"
      "                             b or B - bits/bytes\n"
      "                             k or K - kilo/kibi\n"
      "                             m or M - mega/mebi\n"
      "                             g or G - giga/gibi\n"
      "                             t or T - tera/tebi\n"
      "                           Whether the result is in metric or binary\n"
      "                           depends on their respective options above\n"
      "  -S, --small            Perform test using a small size download "
      "[about 13MB]\n"
      "                           Fastest default test but with the least\n"
      "                           accurate results\n"
      "  -M, --medium           Perform test using a medium size download "
      "[about 40MB]\n"
      "                           Slower than the default small test but\n"
      "                           with slightly more accurate results\n"
      "  -L, --large            Perform test using a large size download "
      "[about 82MB]\n"
      "                           Slowest default test but with the most\n"
      "                           accurate results\n"
      "  -u URL, --url=URL      Perform test with URL instead of the "
      "default\n"
      "  -?, -h, --help         Show this message and exit\n"
      "  -v, --version          Show version information and exit\n"
      "Defaults:\n"
      "  If neither options `--bit' nor `--byte' are given, `--byte' is "
      "implied\n"
      "  If neither options `--metric' nor `--binary' are given, `--metric' "
      "is implied\n"
      "  If none of the options `--small', `--medium' nor `--large' are\n"
      "    given, `--medium' is implied\n",
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

static dsp_boolean_t
dsp_set_unit_option (char c)
{
  switch (c)
  {
    case 'b':
    case 'B':
      unit_option = DSP_UNIT_OPTION_B;
      break;
    case 'k':
    case 'K':
      unit_option = DSP_UNIT_OPTION_K;
      break;
    case 'm':
    case 'M':
      unit_option = DSP_UNIT_OPTION_M;
      break;
    case 'g':
    case 'G':
      unit_option = DSP_UNIT_OPTION_G;
      break;
    case 't':
    case 'T':
      unit_option = DSP_UNIT_OPTION_T;
      break;
    default:
      return DSP_FALSE;
  }
  return DSP_TRUE;
}

static void
dsp_parse_opts (char **v)
{
  size_t x;
  size_t y;
  char *s;

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
    else if (strncmp (v[x], "--url=", strlen ("--url=")) == 0)
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
    else if (dsp_streq (v[x], "-n") || dsp_streq (v[x], "--unit"))
    {
      if (!v[x + 1] || v[x + 1][0] == '-')
      {
        dsp_perror ("`%s' requires an argument", v[x]);
        dsp_usage (DSP_TRUE);
        exit (EXIT_FAILURE);
      }
      else
      {
        if (!dsp_set_unit_option (v[x + 1][0]))
        {
          dsp_perror ("`%s' is not a valid argument for `%s'",
              v[x + 1], v[x]);
          dsp_usage (DSP_TRUE);
          exit (EXIT_FAILURE);
        }
        ++x;
      }
    }
    else if (strncmp (v[x], "--unit=", strlen ("--unit=")) == 0)
    {
      s = strchr (v[x], '=');
      if (!s || !*s)
      {
        dsp_perror ("`--unit' requires an argument");
        dsp_usage (DSP_TRUE);
        exit (EXIT_FAILURE);
      }
      ++s;
      if (!dsp_set_unit_option (*s))
      {
        dsp_perror ("`%s' is not a valid argument for `--unit'", s);
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
            case 'n':
              if (!v[x][y + 1])
              {
                dsp_perror ("`-n' requires an argument");
                dsp_usage (DSP_TRUE);
                exit (EXIT_FAILURE);
              }
              else if (!dsp_set_unit_option (v[x][y + 1]))
              {
                dsp_perror ("`%c' is not a valid argument for `-n'",
                    v[x][y + 1]);
                dsp_usage (DSP_TRUE);
                exit (EXIT_FAILURE);
              }
              ++y;
              break;
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
          (medium_test) ? 'M' : 'L',
          (medium_test) ? "medium" : "large");
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
      dsp_perror ("`-%c'/`--%s' and `-%c'/`--%s' are mutually exclusive",
          (small_test) ? 'S' : (medium_test) ? 'M' : 'L',
          (small_test) ? "small" : (medium_test) ? "medium" : "large",
          'u', "url");
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

  bits = (bytes * ((dsp_byte_t) CHAR_BIT));
  buf[0] = '\0';

  switch (unit_option)
  {
    case DSP_UNIT_OPTION_B:
    {
      if (use_byte)
        snprintf (buf, n, "%lu " DSP_BYTE_SYMBOL, bytes);
      else
        snprintf (buf, n, "%lu " DSP_BIT_SYMBOL, bits);
      break;
    }
#define __DSP_DIVDBL(__b, __v) (((double) __b) / ((double) __v))
    case DSP_UNIT_OPTION_K:
    {
      if (use_byte)
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_KILOBYTE_SYMBOL,
              __DSP_DIVDBL (bytes, DSP_METRIC_KILO));
        else
          snprintf (buf, n, "%g " DSP_BINARY_KIBIBYTE_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_KIBI));
      }
      else
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_KILOBIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_METRIC_KILO));
        else
          snprintf (buf, n, "%g " DSP_BINARY_KIBIBIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_KIBI));
      }
      break;
    }
    case DSP_UNIT_OPTION_M:
    {
      if (use_byte)
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_MEGABYTE_SYMBOL,
              __DSP_DIVDBL (bytes, DSP_METRIC_MEGA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_MEBIBYTE_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_MEBI));
      }
      else
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_MEGABIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_METRIC_MEGA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_MEBIBIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_MEBI));
      }
      break;
    }
    case DSP_UNIT_OPTION_G:
    {
      if (use_byte)
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_GIGABYTE_SYMBOL,
              __DSP_DIVDBL (bytes, DSP_METRIC_GIGA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_GIBIBYTE_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_GIBI));
      }
      else
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_GIGABIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_METRIC_GIGA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_GIBIBIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_GIBI));
      }
      break;
    }
    case DSP_UNIT_OPTION_T:
    {
      if (use_byte)
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_TERABYTE_SYMBOL,
              __DSP_DIVDBL (bytes, DSP_METRIC_TERA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_TEBIBYTE_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_TEBI));
      }
      else
      {
        if (use_metric)
          snprintf (buf, n, "%g " DSP_METRIC_TERABIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_METRIC_TERA));
        else
          snprintf (buf, n, "%g " DSP_BINARY_TEBIBIT_SYMBOL,
              __DSP_DIVDBL (bits, DSP_BINARY_TEBI));
      }
      break;
    }
    default:
    {
      if (use_byte &&
          ((use_metric && (bytes < DSP_METRIC_KILO)) ||
           (use_binary && (bytes < DSP_BINARY_KIBI))))
      {
        snprintf (buf, n, "%lu " DSP_BYTE_SYMBOL, bytes);
        break;
      }
      if (use_bit &&
          ((use_metric && (bits < DSP_METRIC_KILO)) ||
           (use_binary && (bits < DSP_BINARY_KIBI))))
      {
        snprintf (buf, n, "%lu " DSP_BIT_SYMBOL, bits);
        break;
      }
      if (use_byte)
      {
#define __DSP_SFMT(__s, __v) \
  snprintf (buf, n, "%.2f " __s, __DSP_DIVDBL (bytes, __v))
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
      break;
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
dsp_console_width (void)
{
  int width;
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO x;

  if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &x))
    width = x.dwSize.X;
#else
  struct winsize x;

  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &x) != -1)
    width = x.ws_col;
#endif
  else
    width = DSP_DEFAULT_CONSOLE_WIDTH;

  return width;
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
  static dsp_boolean_t one_sec_passed = DSP_FALSE;

  int i;
  int j;
  double x;
  double elapsed;
  time_t now_time;
  dsp_byte_t now_bytes;
  dsp_byte_t bytes_this_sec;

  if (!total_bytes && (d_total > 0.0))
    total_bytes = ((dsp_byte_t) d_total);

  now_time = -1;
  elapsed = -1.0;

  if (last_time)
  {
    now_time = time (NULL);
    elapsed = (((double) now_time) - ((double) last_time));
    if (elapsed >= 1)
    {
      one_sec_passed = DSP_TRUE;
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
    else
      one_sec_passed = DSP_FALSE;
  }
  else
    last_time = time (NULL);

  fputs ("Calculating... (", stdout);

  x = (d_current / d_total);
  j = 0;

  if (dsp_nan_value (x) || dsp_nan_value ((x * 100)))
  {
    fputs ("0%", stdout);
    j = 1;
  }
  else
  {
    fprintf (stdout, "%.0f%%", (x * 100));
    if ((x * 100) < 10)
      j = 1;
    else if ((x * 100) < 100)
      j = 2;
    else
      j = 3;
  }

  fputc (')', stdout);

  if (one_sec_passed && elapsed && (d_total > d_current))
  {
    if (j == 1)
      fputs ("   ", stdout);
    else if (j == 2)
      fputs ("  ", stdout);
    else
      fputc (' ', stdout);
    i = (d_current / (now_time - start_time));
    if (i < 0)
      i = -i;
    fprintf (stdout, "%02i:%02i  ",
        (((int)((d_total - d_current) / i)) / DSP_SECONDS_IN_MINUTE),
        (((int)((d_total - d_current) / i)) % DSP_MOD_VALUE_FOR_SECONDS));
  }

  j = dsp_console_width ();
  if ((d_current == d_total) && (d_current > 0.0) && (d_total > 0.0))
  {
    fputc ('\r', stdout);
    for (i = 0; i < j; ++i)
      fputc (' ', stdout);
  }

  fputc ('\r', stdout);
  fflush (stdout);
  return 0;
}

static void
dsp_set_temp_file_path (void)
{
  size_t n;
  size_t n_home;
  char *home;

#ifdef _WIN32
  home = _getenv ("USERPROFILE");
#else
  home = getenv ("HOME");
#endif
  n = strlen (DSP_TEMP_FILENAME);

  if (home && *home)
  {
    n_home = strlen (home);
    temp_file_path = (char *) malloc (n + n_home + 2);
    if (temp_file_path)
    {
      strncpy (temp_file_path, home, n_home);
#ifdef _WIN32
      temp_file_path[n_home] = '\\';
#else
      temp_file_path[n_home] = '/';
#endif
      strncpy (temp_file_path + (n_home + 1), DSP_TEMP_FILENAME, n);
      temp_file_path[n_home + n + 1] = '\0';
    }
    else
      goto try_current_directory;
  }
  else
    goto try_current_directory;

  return;

try_current_directory:
  temp_file_path = (char *) malloc (n + 1);
  if (temp_file_path)
  {
    strncpy (temp_file_path, DSP_TEMP_FILENAME, n);
    temp_file_path[n] = '\0';
  }
  else
    dsp_perror (strerror (errno));
  return;
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

  dsp_set_temp_file_path ();
  if (!temp_file_path || !*temp_file_path)
  {
    curl_easy_cleanup (*cp);
    return DSP_FALSE;
  }

  *fp = fopen (temp_file_path, "w+b");
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

static void
dsp_cleanup (void)
{
  struct stat s;

  if (temp_file_path)
  {
    memset (&s, 0, sizeof (struct stat));
    if (stat (temp_file_path, &s) == 0)
    {
#ifdef _WIN32
      if (!DeleteFile (temp_file_path))
#else
      if (unlink (temp_file_path) != 0)
#endif
        dsp_perror ("failed to delete downloaded temporary "
            "file located at `%s' (%s)", temp_file_path, strerror (errno));
    }
    free (temp_file_path);
  }
}

int
main (int argc, char **argv)
{
  atexit (dsp_cleanup);
  dsp_parse_opts (argv);
  dsp_perform ();
  exit (EXIT_SUCCESS);
  return 0; /* for compilers */
}


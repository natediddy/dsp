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

#define DSP_DEFAULT_PROGRAM_NAME "dsp"

#define DSP_VERSION "1.2.0"

#define DSP_USER_AGENT \
    DSP_DEFAULT_PROGRAM_NAME " (Download SPeed tester)/" DSP_VERSION

#define DSP_HELP_OUTPUT \
"Options:\n" \
"  -b, --bit              Show result measurements in bits\n" \
"  -B, --byte             Show result measurements in bytes\n" \
"  -m, --metric           Use metric measurements (multiples of 1000)\n" \
"  -i, --binary           Use binary measurements (multiples of 1024)\n" \
"  -n UNIT, --unit=UNIT   Measure results stricty in UNIT\n" \
"                         UNIT can be any of the following:\n" \
"                             b or B - bits/bytes\n" \
"                             k or K - kilo/kibi\n" \
"                             m or M - mega/mebi\n" \
"                             g or G - giga/gibi\n" \
"                             t or T - tera/tebi\n" \
"                         Whether the result is in metric or binary\n" \
"                         depends on their respective given options above.\n"\
"  -S, --small            Perform test using a small size download (13MB)\n" \
"                         Fastest default test with least accurate results\n"\
"  -M, --medium           Perform test using a medium size download (40MB)\n"\
"                         Slower than `-S' test with more accurate results\n"\
"  -L, --large            Perform test using a large size download (82MB)\n" \
"                         Slowest test but with most accurate results\n" \
"  -u URL, --url=URL      Perform test with URL instead of the default\n" \
"  -?, -h, --help         Show this message and exit\n" \
"  -v, --version          Show version information and exit\n" \
"NOTES:\n" \
"  - If neither options `--bit' nor `--byte' are given, `--byte' is " \
"implied.\n" \
"  - If neither options `--metric' nor `--binary' are given, `--metric' is\n" \
"    implied.\n" \
"  - If none of the options `--small', `--medium' nor `--large' are given,\n" \
"    `--medium' is implied"

#define DSP_VERSION_OUTPUT \
DSP_DEFAULT_PROGRAM_NAME " " DSP_VERSION "\n" \
"Copyright (C) 2013 Nathan Forbes <me@nathanforbes.com>\n" \
"License GPLv3+: GNU GPL version 3 or later" \
"<http://gnu.org/licenses/gpl.html>\n" \
"This is free software: you are free to change and redistribute it.\n" \
"There is NO WARRANTY, to the extent permitted by law.\n" \
"\n" \
"Written by Nathan Forbes."

/*
 * GNU's servers are pretty reliable and these
 * files should (hopefully) be here for while. 
 *
 * The sizes of the files below are 12.87MB, 39.67MB and 82.14MB
 * for the small, medium and large respectively.
 */
#define DSP_DEFAULT_URL_SMALL "http://ftp.gnu.org/gnu/gcc/gcc-2.95.1.tar.gz"
#define DSP_DEFAULT_URL_MEDIUM \
    "http://ftp.gnu.org/gnu/gcc/gcc-4.7.0/gcc-4.6.3-4.7.0.diff.gz"
#define DSP_DEFAULT_URL_LARGE \
    "http://ftp.gnu.org/gnu/gcc/gcc-4.4.5/gcc-4.4.5.tar.gz"

/* tags for final output */
#define DSP_TOTAL_DOWN_TIME_DISPLAY_TAG   "Total d/l time:   "
#define DSP_TOTAL_DOWN_SIZE_DISPLAY_TAG   "Total d/l size:   "
#define DSP_AVERAGE_DOWN_RATE_DISPLAY_TAG "Average d/l rate: "
#define DSP_PEAK_DOWN_RATE_DISPLAY_TAG    "Peak d/l rate:    "
#define DSP_LOWEST_DOWN_RATE_DISPLAY_TAG  "Lowest d/l rate:  "

/* show this if the result data was not set for some reason */
#define DSP_UNKNOWN_DISPLAY_DATA "(unknown)"

#define DSP_BYTE_SYMBOL "B"
#define DSP_BIT_SYMBOL  "bit"

#define DSP_METRIC_KILO_SYMBOL "k"
#define DSP_BINARY_KILO_SYMBOL "K"

#define DSP_MEGA_SYMBOL "M"
#define DSP_GIGA_SYMBOL "G"
#define DSP_TERA_SYMBOL "T"

#define DSP_BINARY_KIBIBYTE_SYMBOL DSP_BINARY_KILO_SYMBOL "i" DSP_BYTE_SYMBOL
#define DSP_BINARY_MEBIBYTE_SYMBOL DSP_MEGA_SYMBOL "i" DSP_BYTE_SYMBOL
#define DSP_BINARY_GIBIBYTE_SYMBOL DSP_GIGA_SYMBOL "i" DSP_BYTE_SYMBOL
#define DSP_BINARY_TEBIBYTE_SYMBOL DSP_TERA_SYMBOL "i" DSP_BYTE_SYMBOL

#define DSP_BINARY_KIBIBIT_SYMBOL DSP_BINARY_KILO_SYMBOL "i" DSP_BIT_SYMBOL
#define DSP_BINARY_MEBIBIT_SYMBOL DSP_MEGA_SYMBOL "i" DSP_BIT_SYMBOL
#define DSP_BINARY_GIBIBIT_SYMBOL DSP_GIGA_SYMBOL "i" DSP_BIT_SYMBOL
#define DSP_BINARY_TEBIBIT_SYMBOL DSP_TERA_SYMBOL "i" DSP_BIT_SYMBOL

#define DSP_METRIC_KILOBYTE_SYMBOL DSP_METRIC_KILO_SYMBOL DSP_BYTE_SYMBOL
#define DSP_METRIC_MEGABYTE_SYMBOL DSP_MEGA_SYMBOL DSP_BYTE_SYMBOL
#define DSP_METRIC_GIGABYTE_SYMBOL DSP_GIGA_SYMBOL DSP_BYTE_SYMBOL
#define DSP_METRIC_TERABYTE_SYMBOL DSP_TERA_SYMBOL DSP_BYTE_SYMBOL

#define DSP_METRIC_KILOBIT_SYMBOL DSP_METRIC_KILO_SYMBOL DSP_BIT_SYMBOL
#define DSP_METRIC_MEGABIT_SYMBOL DSP_MEGA_SYMBOL DSP_BIT_SYMBOL
#define DSP_METRIC_GIGABIT_SYMBOL DSP_GIGA_SYMBOL DSP_BIT_SYMBOL
#define DSP_METRIC_TERABIT_SYMBOL DSP_TERA_SYMBOL DSP_BIT_SYMBOL

/* static buffer sizes */
#define DSP_TIME_BUFFER_SIZE           64
#define DSP_DATE_BUFFER_SIZE           12
#define DSP_SIZE_BUFFER_SIZE           32
#define DSP_SPEED_BUFFER_SIZE          36
#define DSP_TEMP_FILENAME_BUFFER_SIZE 256

/* use this if the width of the console can't be determined */
#define DSP_FALLBACK_CONSOLE_WIDTH 40

/* constants for dealing with time stuff */
#define DSP_SECONDS_IN_DAY    86400
#define DSP_SECONDS_IN_HOUR    3600
#define DSP_SECONDS_IN_MINUTE    60

#define DSP_MOD_VALUE_FOR_SECONDS 60

/* system specific utilities */
/* {{{ */
#ifdef _WIN32
# define DSP_PATH_SEPARATOR_CHAR   '\\'
# define DSP_USER_HOME_PATH_VAR    "USERPROFILE"
# define DSP_GETENV(varname)       _getenv(varname)
# define DSP_DELETE_FILE(filename) (DeleteFile(filename))
#else
# define DSP_PATH_SEPARATOR_CHAR   '/'
# define DSP_USER_HOME_PATH_VAR    "HOME"
# define DSP_GETENV(varname)       getenv(varname)
# define DSP_DELETE_FILE(filename) (unlink(filename) == 0)
#endif

#define DSP_GETENV_HOME DSP_GETENV(DSP_USER_HOME_PATH_VAR)
/* }}} */

/* use a custom boolean type so there's no
   need to depend on the system's libc boolean type */
/* {{{ */
typedef unsigned char dsp_boolean_t;

#define DSP_FALSE ((dsp_boolean_t)0)
#define DSP_TRUE  ((dsp_boolean_t)1)
/* }}} */


/* custom byte type for when dealing
   specifically with values pertaining to bytes */
/* {{{ */
typedef unsigned long dsp_byte_t;

/* initialization value */
#define DSP_ZERO_BYTES ((dsp_byte_t)0LU)

/* units defined by the International Electrotechnical Commission (IEC) */
#define DSP_BINARY_KIBI ((dsp_byte_t)1024LU)
#define DSP_BINARY_MEBI ((dsp_byte_t)1048576LU)
#define DSP_BINARY_GIBI ((dsp_byte_t)1073741824LU)
#define DSP_BINARY_TEBI ((dsp_byte_t)1099511627776LU)

/* units defined by the International System of Units (SI) */
#define DSP_METRIC_KILO ((dsp_byte_t)1000LU)
#define DSP_METRIC_MEGA ((dsp_byte_t)1000000LU)
#define DSP_METRIC_GIGA ((dsp_byte_t)1000000000LU)
#define DSP_METRIC_TERA ((dsp_byte_t)1000000000000LU)
/* }}} */

/* custom type for the -n/--unit command */
/* {{{ */
typedef unsigned int dsp_unit_option_t;

#define DSP_UNIT_OPTION_0 0
#define DSP_UNIT_OPTION_B 1
#define DSP_UNIT_OPTION_K 2
#define DSP_UNIT_OPTION_M 3
#define DSP_UNIT_OPTION_G 4
#define DSP_UNIT_OPTION_T 5
/* }}} */

const char *program_name;

char *            user_supplied_url   = NULL;
char *            temp_file_path      = NULL;
time_t            start_time          = -1;
time_t            end_time            = -1;
dsp_boolean_t     use_bit             = DSP_FALSE;
dsp_boolean_t     use_byte            = DSP_FALSE;
dsp_boolean_t     use_metric          = DSP_FALSE;
dsp_boolean_t     use_binary          = DSP_FALSE;
dsp_boolean_t     small_test          = DSP_FALSE;
dsp_boolean_t     medium_test         = DSP_FALSE;
dsp_boolean_t     large_test          = DSP_FALSE;
dsp_byte_t        total_bytes         = DSP_ZERO_BYTES;
dsp_byte_t        most_bytes_per_sec  = DSP_ZERO_BYTES;
dsp_byte_t        least_bytes_per_sec = DSP_ZERO_BYTES;
dsp_unit_option_t unit_option         = DSP_UNIT_OPTION_0;

struct {
    char total_down_time[DSP_TIME_BUFFER_SIZE];
    char total_down_size[DSP_SIZE_BUFFER_SIZE];
    char average_down_rate[DSP_SPEED_BUFFER_SIZE];
    char peak_down_rate[DSP_SPEED_BUFFER_SIZE];
    char lowest_down_rate[DSP_SPEED_BUFFER_SIZE];
} display_data;

static void dsp_show_usage(dsp_boolean_t error)
{
    fprintf((!error) ? stdout : stderr,
            "Usage: %s -[bBiLmMS] [--unit=UNIT] [--url=URL]\n",
            program_name);
}

static void dsp_show_help(void)
{
    dsp_show_usage(DSP_FALSE);
    puts(DSP_HELP_OUTPUT);
    exit(EXIT_SUCCESS);
}

static void dsp_show_version(void)
{
    puts(DSP_VERSION_OUTPUT);
    exit(EXIT_SUCCESS);
}

static void dsp_print_error(const char *str, ...)
{
    size_t x;
    va_list args;
    dsp_boolean_t fmt;

    fputs(program_name, stderr);
    fputs(": error: ", stderr);

    fmt = DSP_FALSE;
    for (x = 0; str[x]; ++x) {
        if (str[x] == '%') {
            fmt = DSP_TRUE;
            break;
        }
    }

    if (!fmt)
        fputs(str, stderr);
    else {
        va_start(args, str);
        vfprintf(stderr, str, args);
        va_end(args);
    }
    fputc('\n', stderr);
}

static unsigned int dsp_get_random_uint(void)
{
    unsigned int x;

    x = ((((unsigned int)time(NULL)) * 1765301923) + 12345);
    return ((x << 16) | ((x >> 16) & 0xffff));
}

static dsp_boolean_t dsp_are_strings_equal(const char *string1,
                                           const char *string2)
{
    size_t n;

    n = strlen(string1);
    if ((n == strlen(string2)) && (memcmp(string1, string2, n) == 0))
        return DSP_TRUE;
    return DSP_FALSE;
}

static dsp_boolean_t dsp_does_string_start_with(const char *string,
                                                const char *prefix)
{
    size_t n;

    n = strlen(prefix);
    if ((strlen(string) >= n) && (memcmp(string, prefix, n) == 0))
        return DSP_TRUE;
    return DSP_FALSE;
}

static void dsp_set_proper_program_name(char *argv0)
{
    char *x;

    /* don't use DSP_PATH_SEPARATOR_CHAR
       here since Windows can check both */
    if (argv0 && *argv0) {
        x = strrchr(argv0, '/');
        if (x) {
            program_name = ++x;
            return;
        }
#ifdef _WIN32
        x = strrchr(argv0, '\\');
        if (x) {
            program_name = ++x;
            return;
        }
#endif
        program_name = argv0;
        return;
    }
    program_name = DSP_DEFAULT_PROGRAM_NAME;
}

static dsp_boolean_t dsp_set_unit_option(char c)
{
    switch (c) {
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

static void dsp_parse_options(char **v)
{
    size_t x;
    size_t y;
    char *s;

    dsp_set_proper_program_name(v[0]);

    if (!v[1]) {
        use_byte = DSP_TRUE;
        use_metric = DSP_TRUE;
        medium_test = DSP_TRUE;
        return;
    }

    for (x = 1; v[x]; ++x) {
        if (dsp_are_strings_equal(v[x], "-?") ||
                dsp_are_strings_equal(v[x], "-h") ||
                dsp_are_strings_equal(v[x], "--help"))
            dsp_show_help();
        else if (dsp_are_strings_equal(v[x], "-v") ||
                dsp_are_strings_equal(v[x], "--version"))
            dsp_show_version();
        else if (dsp_are_strings_equal(v[x], "-b") ||
                dsp_are_strings_equal(v[x], "--bit"))
            use_bit = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-B") ||
                dsp_are_strings_equal(v[x], "--byte"))
            use_byte = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-m") ||
                dsp_are_strings_equal(v[x], "--metric"))
            use_metric = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-i") ||
                dsp_are_strings_equal(v[x], "--binary"))
            use_binary = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-S") ||
                dsp_are_strings_equal(v[x], "--small"))
            small_test = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-M") ||
                dsp_are_strings_equal(v[x], "--medium"))
            medium_test = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-L") ||
                dsp_are_strings_equal(v[x], "--large"))
            large_test = DSP_TRUE;
        else if (dsp_are_strings_equal(v[x], "-u") ||
                dsp_are_strings_equal(v[x], "--url")) {
            if (!v[x + 1] || v[x + 1][0] == '-') {
                dsp_print_error("`%s' requires an argument", v[x]);
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            } else {
                user_supplied_url = v[++x];
                continue;
            }
        } else if (dsp_does_string_start_with(v[x], "--url=")) {
            user_supplied_url = strchr(v[x], '=');
            if (user_supplied_url)
                ++user_supplied_url;
            if (!user_supplied_url || !*user_supplied_url) {
                dsp_print_error("`--url' requires an argument");
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            }
        } else if (dsp_are_strings_equal(v[x], "-n") ||
                dsp_are_strings_equal(v[x], "--unit")) {
            if (!v[x + 1] || v[x + 1][0] == '-') {
                dsp_print_error("`%s' requires an argument", v[x]);
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            } else {
                if (!dsp_set_unit_option(v[x + 1][0])) {
                    dsp_print_error("`%s' is not a valid argument for `%s'",
                            v[x + 1], v[x]);
                    dsp_show_usage(DSP_TRUE);
                    exit(EXIT_FAILURE);
                }
                ++x;
            }
        } else if (dsp_does_string_start_with(v[x], "--unit=")) {
            s = strchr(v[x], '=');
            if (!s || !*s) {
                dsp_print_error("`--unit' requires an argument");
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            }
            ++s;
            if (!dsp_set_unit_option(*s)) {
                dsp_print_error("`%s' is not a valid argument for `--unit'",
                        s);
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            }
        } else {
            if (v[x][0] == '-') {
                for (y = 1; v[x][y]; ++y) {
                    switch (v[x][y]) {
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
                        dsp_print_error("`-u' requires an argument");
                        dsp_show_usage(DSP_TRUE);
                        exit(EXIT_FAILURE);
                    case 'n':
                        if (!v[x][y + 1]) {
                            dsp_print_error("`-n' requires an argument");
                            dsp_show_usage(DSP_TRUE);
                            exit(EXIT_FAILURE);
                        } else if (!dsp_set_unit_option(v[x][y + 1])) {
                            dsp_print_error("`%c' is not a valid argument "
                                    "for `-n'", v[x][y + 1]);
                            dsp_show_usage(DSP_TRUE);
                            exit(EXIT_FAILURE);
                        }
                        ++y;
                        break;
                    default:
                        dsp_print_error("unrecognized option `-%c'", v[x][y]);
                        dsp_show_usage(DSP_TRUE);
                        exit(EXIT_FAILURE);
                    }
                }
            } else {
                dsp_print_error("unrecognized option `%s'", v[x]);
                dsp_show_usage(DSP_TRUE);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (use_bit && use_byte) {
        dsp_print_error("`-b'/`--bit' and `-B'/`--byte' are mutually "
                "exclusive");
        exit(EXIT_FAILURE);
    }

    if (use_metric && use_binary) {
        dsp_print_error("`-m'/`--metric' and `-i'/`--binary' are mutually "
                "exclusive");
        exit(EXIT_FAILURE);
    }

    if (small_test && medium_test && large_test) {
        dsp_print_error ("`-S'/`--small', `-M'/`--medium' and `-L'/`--large' "
                "are all mutually exclusive");
        exit(EXIT_FAILURE);
    }

    if (small_test) {
        if (medium_test || large_test) {
            dsp_print_error("`-S'/`--small' and `-%c'/`--%s' are mutually "
                    "exclusive",
                    (medium_test) ? 'M' : 'L',
                    (medium_test) ? "medium" : "large");
            exit(EXIT_FAILURE);
        }
    }

    if (medium_test && large_test) {
        dsp_print_error("`-M'/`--medium' and `-L'/`--large' are mutually "
                "exclusive");
        exit(EXIT_FAILURE);
    }

    if (user_supplied_url && *user_supplied_url) {
        if (small_test || medium_test || large_test) {
            dsp_print_error("`-%c'/`--%s' and `-u'/`--url' are mutually "
                    "exclusive",
                    (small_test) ? 'S' : (medium_test) ? 'M' : 'L',
                    (small_test) ? "small" : (medium_test) ? "medium" :
                    "large");
            exit(EXIT_FAILURE);
        }
    }

    if (!small_test && !medium_test && !large_test)
        medium_test = DSP_TRUE;

    if (!use_bit && !use_byte)
        use_byte = DSP_TRUE;

    if (!use_metric && !use_binary)
        use_metric = DSP_TRUE;
}

static void dsp_format_size(char *buffer, size_t n, dsp_byte_t bytes)
{
    dsp_byte_t bits;

    bits = (bytes * ((dsp_byte_t)CHAR_BIT));
    buffer[0] = '\0';

    switch (unit_option) {
    case DSP_UNIT_OPTION_B: {
        if (use_byte)
            snprintf(buffer, n, "%lu " DSP_BYTE_SYMBOL, bytes);
        else
            snprintf(buffer, n, "%lu " DSP_BIT_SYMBOL, bits);
        break;
    }
#define __DSP_DIVDBL(__b, __v) (((double)__b) / ((double)__v))
    case DSP_UNIT_OPTION_K: {
        if (use_byte) {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_KILOBYTE_SYMBOL,
                        __DSP_DIVDBL(bytes, DSP_METRIC_KILO));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_KIBIBYTE_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_KIBI));
        } else {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_KILOBIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_METRIC_KILO));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_KIBIBIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_KIBI));
        }
        break;
    }
    case DSP_UNIT_OPTION_M: {
        if (use_byte) {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_MEGABYTE_SYMBOL,
                        __DSP_DIVDBL(bytes, DSP_METRIC_MEGA));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_MEBIBYTE_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_MEBI));
        } else {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_MEGABIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_METRIC_MEGA));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_MEBIBIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_MEBI));
        }
        break;
    }
    case DSP_UNIT_OPTION_G: {
        if (use_byte) {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_GIGABYTE_SYMBOL,
                        __DSP_DIVDBL(bytes, DSP_METRIC_GIGA));
            else
                snprintf (buffer, n, "%g " DSP_BINARY_GIBIBYTE_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_GIBI));
        } else {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_GIGABIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_METRIC_GIGA));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_GIBIBIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_GIBI));
        }
        break;
    }
    case DSP_UNIT_OPTION_T: {
        if (use_byte) {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_TERABYTE_SYMBOL,
                        __DSP_DIVDBL(bytes, DSP_METRIC_TERA));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_TEBIBYTE_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_TEBI));
        } else {
            if (use_metric)
                snprintf(buffer, n, "%g " DSP_METRIC_TERABIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_METRIC_TERA));
            else
                snprintf(buffer, n, "%g " DSP_BINARY_TEBIBIT_SYMBOL,
                        __DSP_DIVDBL(bits, DSP_BINARY_TEBI));
        }
        break;
    }
    default: {
        if (use_byte &&
                ((use_metric && (bytes < DSP_METRIC_KILO)) ||
                 (use_binary && (bytes < DSP_BINARY_KIBI))))
        {
            snprintf(buffer, n, "%lu " DSP_BYTE_SYMBOL, bytes);
            break;
        }
        if (use_bit &&
                ((use_metric && (bits < DSP_METRIC_KILO)) ||
                 (use_binary && (bits < DSP_BINARY_KIBI))))
        {
            snprintf(buffer, n, "%lu " DSP_BIT_SYMBOL, bits);
            break;
        }
        if (use_byte) {
#define __DSP_SFMT(__s, __v) \
            snprintf(buffer, n, "%.1f " __s, __DSP_DIVDBL(bytes, __v))
            if (use_metric) {
                if ((bytes / DSP_METRIC_TERA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_TERABYTE_SYMBOL, DSP_METRIC_TERA);
                else if ((bytes / DSP_METRIC_GIGA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_GIGABYTE_SYMBOL, DSP_METRIC_GIGA);
                else if ((bytes / DSP_METRIC_MEGA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_MEGABYTE_SYMBOL, DSP_METRIC_MEGA);
                else if ((bytes / DSP_METRIC_KILO) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_KILOBYTE_SYMBOL, DSP_METRIC_KILO);
            } else {
                if ((bytes / DSP_BINARY_TEBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_TEBIBYTE_SYMBOL, DSP_BINARY_TEBI);
                else if ((bytes / DSP_BINARY_GIBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_GIBIBYTE_SYMBOL, DSP_BINARY_GIBI);
                else if ((bytes / DSP_BINARY_MEBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_MEBIBYTE_SYMBOL, DSP_BINARY_MEBI);
                else if ((bytes / DSP_BINARY_KIBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_KIBIBYTE_SYMBOL, DSP_BINARY_KIBI);
            }
        } else {
#undef __DSP_SFMT
#define __DSP_SFMT(__s, __v) snprintf(buffer, n, "%lu " __s, (bits / __v))
            if (use_metric) {
                if ((bits / DSP_METRIC_TERA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_TERABIT_SYMBOL, DSP_METRIC_TERA);
                else if ((bits / DSP_METRIC_GIGA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_GIGABIT_SYMBOL, DSP_METRIC_GIGA);
                else if ((bits / DSP_METRIC_MEGA) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_MEGABIT_SYMBOL, DSP_METRIC_MEGA);
                else if ((bits / DSP_METRIC_KILO) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_METRIC_KILOBIT_SYMBOL, DSP_METRIC_KILO);
            } else {
                if ((bits / DSP_BINARY_TEBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_TEBIBIT_SYMBOL, DSP_BINARY_TEBI);
                else if ((bits / DSP_BINARY_GIBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_GIBIBIT_SYMBOL, DSP_BINARY_GIBI);
                else if ((bits / DSP_BINARY_MEBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_MEBIBIT_SYMBOL, DSP_BINARY_MEBI);
                else if ((bits / DSP_BINARY_KIBI) > DSP_ZERO_BYTES)
                    __DSP_SFMT(DSP_BINARY_KIBIBIT_SYMBOL, DSP_BINARY_KIBI);
            }
        }
        break;
    } /* default: */
    } /* switch(unit_option) */
}

static void dsp_format_rate(char *buffer,
                            size_t n,
                            dsp_boolean_t for_average,
                            dsp_byte_t bytes)
{
    int elapsed;
    size_t x;
    dsp_byte_t t_bytes;

    if (for_average) {
        elapsed = (((int)start_time) - ((int)end_time));
        if (elapsed < 0)
            elapsed = -elapsed;
        t_bytes = (bytes / ((dsp_byte_t)elapsed));
        dsp_format_size(buffer, DSP_SIZE_BUFFER_SIZE, t_bytes);
    } else
        dsp_format_size(buffer, DSP_SIZE_BUFFER_SIZE, bytes);

    if (*buffer) {
        x = strlen(buffer);
        buffer[x++] = '/';
        buffer[x++] = 's';
        buffer[x] = '\0';
    }
}

static void dsp_format_time(char *buffer, size_t n)
{
    int elapsed;
    int days;
    int hours;
    int minutes;
    int seconds;
    size_t x;

    elapsed = (((int)start_time) - ((int)end_time));
    if (elapsed < 0)
        elapsed = -elapsed;

    days = (elapsed / DSP_SECONDS_IN_DAY);
    hours = (elapsed / DSP_SECONDS_IN_HOUR);
    minutes = (elapsed / DSP_SECONDS_IN_MINUTE);
    seconds = (elapsed % DSP_MOD_VALUE_FOR_SECONDS);
    x = 0;

    if (days > 0) {
        snprintf(buffer, n, "%i day%s", days, (days == 1) ? "" : "s");
        x = strlen(buffer);
        if ((hours == 0) && (minutes == 0) && (seconds == 0))
            return;
    }

    if (hours > 0) {
        snprintf(buffer, n, "%s%i hour%s",
                (days > 0) ? " " : "", hours, (hours == 1) ? "" : "s");
        x = strlen(buffer);
        if ((minutes == 0) && (seconds == 0))
            return;
    }

    if (minutes > 0) {
        snprintf(buffer + x, n - x, "%s%i minute%s",
                ((days > 0) || (hours > 0)) ? " " : "",
                minutes, (minutes == 1) ? "" : "s");
        x = strlen(buffer);
        if (seconds == 0)
            return;
    }

    if (seconds > 0)
        snprintf(buffer + x, n - x, "%s%i second%s",
                ((days > 0) || (hours > 0) || (minutes > 0)) ? " " : "",
                seconds, (seconds == 1) ? "" : "s");
}

static void dsp_format_date(char *buffer, size_t n)
{
    time_t now;
    struct tm *t;

    now = time(NULL);
    t = localtime(&now);
    if (!t) {
        buffer[0] = '\0';
        return;
    }
    strftime(buffer, n, "%Y-%m-%d", t);
}

static void dsp_format_temp_filename(char *buffer, size_t n)
{
    unsigned int random;
    char *test_name;
    char date[DSP_DATE_BUFFER_SIZE];

    random = dsp_get_random_uint();
    dsp_format_date(date, DSP_DATE_BUFFER_SIZE);

    if (user_supplied_url && *user_supplied_url)
        test_name = "user";
    else if (small_test)
        test_name = "small";
    else if (medium_test)
        test_name = "medium";
    else
        test_name = "large";

    snprintf(buffer, n, ".%s_%s-test_%s_%u",
            DSP_DEFAULT_PROGRAM_NAME, test_name, date, random);
}

static dsp_boolean_t dsp_is_nan_value(double v)
{
    volatile double x;

    x = v;
    if (x != x)
        return DSP_TRUE;
    return DSP_FALSE;
}

static int dsp_get_console_width(void)
{
    int width;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO x;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &x))
        width = x.dwSize.X;
#else
    struct winsize x;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &x) != -1)
        width = x.ws_col;
#endif
    else
        width = DSP_FALLBACK_CONSOLE_WIDTH;
    return width;
}

static int dsp_progress_callback(void *data,
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
    int days_remaining;
    int hours_remaining;
    int minutes_remaining;
    int seconds_remaining;
    int console_width;
    double x;
    double elapsed;
    time_t now_time;
    dsp_byte_t now_bytes;
    dsp_byte_t bytes_this_sec;

    if (!total_bytes && (d_total > 0.0))
        total_bytes = ((dsp_byte_t)d_total);

    now_time = -1;
    elapsed = -1.0;

    if (last_time) {
        now_time = time(NULL);
        elapsed = (((double)now_time) - ((double)last_time));
        if (elapsed >= 1) {
            one_sec_passed = DSP_TRUE;
            now_bytes = ((dsp_byte_t)d_current);
            if (last_bytes == DSP_ZERO_BYTES)
                last_bytes = now_bytes;
            else if (now_bytes > last_bytes) {
                bytes_this_sec = (now_bytes - last_bytes);
                if (bytes_this_sec) {
                    if (bytes_this_sec > most_bytes_per_sec)
                        most_bytes_per_sec = bytes_this_sec;
                    if ((least_bytes_per_sec == DSP_ZERO_BYTES) ||
                            (bytes_this_sec < least_bytes_per_sec))
                        least_bytes_per_sec = bytes_this_sec;
                }
            }
            last_time = now_time;
            last_bytes = now_bytes;
        } else
            one_sec_passed = DSP_FALSE;
    } else
        last_time = time(NULL);

    console_width = dsp_get_console_width();

    fputs("Calculating... (", stdout);
    console_width -= strlen("Calculating... (");

    x = (d_current / d_total);
    j = 0;

    if (dsp_is_nan_value(x) || dsp_is_nan_value((x * 100))) {
        putchar('0');
        j = 1;
    } else {
        printf("%.0f", (x * 100));
        if ((x * 100) < 10)
            j = 1;
        else if ((x * 100) < 100)
            j = 2;
        else
            j = 3;
    }

    console_width -= j;
    fputs("%)", stdout);
    console_width -= 2;

    if (one_sec_passed && elapsed && (d_total > d_current)) {
        if (j == 1) {
            fputs("   ", stdout);
            console_width -= 3;
        } else if (j == 2) {
            fputs("  ", stdout);
            console_width -= 2;
        } else {
            putchar(' ');
            console_width -= 1;
        }
        i = (d_current / (now_time - start_time));
        if (i < 0)
            i = -i;
        days_remaining =
            (((int)((d_total - d_current) / i)) / DSP_SECONDS_IN_DAY);
        hours_remaining =
            (((int)((d_total - d_current) / i)) / DSP_SECONDS_IN_HOUR);
        minutes_remaining =
            (((int)((d_total - d_current) / i)) / DSP_SECONDS_IN_MINUTE);
        seconds_remaining =
            (((int)((d_total - d_current) / i)) % DSP_MOD_VALUE_FOR_SECONDS);
        if (days_remaining > 0)
            console_width -=
                printf("%i day%s", days_remaining,
                        (days_remaining == 1) ? "" : "s");
        if (hours_remaining > 0)
            console_width -=
                printf("%s%i hour%s",
                        (days_remaining > 0) ? " " : "",
                        hours_remaining,
                        (hours_remaining == 1) ? "" : "s");
        if (minutes_remaining > 0)
            console_width -=
                printf("%s%i minute%s",
                        ((days_remaining > 0) ||
                         (hours_remaining > 0)) ? " " : "",
                        minutes_remaining,
                        (minutes_remaining == 1) ? "" : "s");
        if (seconds_remaining > 0)
            console_width -=
                printf("%s%i second%s",
                        ((days_remaining > 0) || (hours_remaining > 0) ||
                         (minutes_remaining > 0)) ? " " : "",
                        seconds_remaining,
                        (seconds_remaining == 1) ? "" : "s");
        for (j = 0; j < console_width; ++j)
            putchar(' ');
    }

    console_width = dsp_get_console_width();
    if ((d_current == d_total) && (d_current > 0.0) && (d_total > 0.0)) {
        putchar('\r');
        for (i = 0; i < console_width; ++i)
            putchar(' ');
    }

    putchar('\r');
    fflush(stdout);
    return 0;
}

static void dsp_set_temp_file_path(void)
{
    size_t n;
    size_t n_home;
    char *home;
    char name[DSP_TEMP_FILENAME_BUFFER_SIZE];

    dsp_format_temp_filename(name, DSP_TEMP_FILENAME_BUFFER_SIZE);
    n = strlen(name);

    home = DSP_GETENV_HOME;
    if (home && *home) {
        n_home = strlen(home);
        temp_file_path = (char *)malloc(n + n_home + 2);
        if (temp_file_path)
            snprintf(temp_file_path, n + n_home + 2, "%s%c%s",
                    home, DSP_PATH_SEPARATOR_CHAR, name);
        else
            goto try_current_directory;
    } else
        goto try_current_directory;

    return;

try_current_directory:
    temp_file_path = (char *)malloc(n + 1);
    if (temp_file_path) {
        strncpy(temp_file_path, name, n);
        temp_file_path[n] = '\0';
    } else
        dsp_print_error(strerror(errno));
    return;
}

static dsp_boolean_t dsp_setup_curl(CURL **cp, FILE **fp)
{
    int s_errno;
    CURLcode c_status;
    const char *url;

    c_status = CURLE_OK;
    *cp = curl_easy_init();

    if (!*cp) {
        c_status = CURLE_FAILED_INIT;
        goto failure;
    }

    if (user_supplied_url && *user_supplied_url)
        url = user_supplied_url;
    else if (small_test)
        url = DSP_DEFAULT_URL_SMALL;
    else if (medium_test)
        url = DSP_DEFAULT_URL_MEDIUM;
    else if (large_test)
        url = DSP_DEFAULT_URL_LARGE;
    else
        url = NULL;

    c_status = curl_easy_setopt(*cp, CURLOPT_URL, url);
    if (c_status != CURLE_OK)
        goto failure;

    c_status = curl_easy_setopt(*cp, CURLOPT_USERAGENT, DSP_USER_AGENT);
    if (c_status != CURLE_OK)
        goto failure;

    c_status = curl_easy_setopt(*cp, CURLOPT_FOLLOWLOCATION, 1L);
    if (c_status != CURLE_OK)
        goto failure;

    dsp_set_temp_file_path();
    if (!temp_file_path || !*temp_file_path) {
        curl_easy_cleanup(*cp);
        return DSP_FALSE;
    }

    *fp = fopen(temp_file_path, "w+b");
    if (!*fp) {
        s_errno = errno;
        dsp_print_error(strerror(s_errno));
        curl_easy_cleanup(*cp);
        return DSP_FALSE;
    }

    c_status = curl_easy_setopt(*cp, CURLOPT_WRITEDATA, (void *)*fp);
    if (c_status != CURLE_OK)
        goto failure;

    c_status = curl_easy_setopt(*cp, CURLOPT_WRITEFUNCTION, (void *)fwrite);
    if (c_status != CURLE_OK)
        goto failure;

    c_status = curl_easy_setopt(*cp, CURLOPT_NOPROGRESS, 0L);
    if (c_status != CURLE_OK)
        goto failure;

    c_status =
        curl_easy_setopt(*cp, CURLOPT_PROGRESSFUNCTION,
                (void *)dsp_progress_callback);
    if (c_status != CURLE_OK)
        goto failure;

    return DSP_TRUE;

failure:
    dsp_print_error(curl_easy_strerror(c_status));
    if (*fp)
        fclose(*fp);
    curl_easy_cleanup(*cp);
    return DSP_FALSE;
}

static void dsp_fill_display_data(void)
{
    display_data.total_down_size[0] = '\0';
    display_data.total_down_time[0] = '\0';
    display_data.average_down_rate[0] = '\0';
    display_data.peak_down_rate[0] = '\0';
    display_data.lowest_down_rate[0] = '\0';

    dsp_format_size(display_data.total_down_size, DSP_SIZE_BUFFER_SIZE,
            total_bytes);
    dsp_format_time(display_data.total_down_time, DSP_TIME_BUFFER_SIZE);
    dsp_format_rate(display_data.average_down_rate, DSP_SPEED_BUFFER_SIZE,
            DSP_TRUE, total_bytes);
    dsp_format_rate(display_data.peak_down_rate, DSP_SPEED_BUFFER_SIZE,
            DSP_FALSE, most_bytes_per_sec);
    dsp_format_rate(display_data.lowest_down_rate, DSP_SPEED_BUFFER_SIZE,
            DSP_FALSE, least_bytes_per_sec);
}

static void dsp_show_display_data(void)
{
    fputs(DSP_TOTAL_DOWN_TIME_DISPLAY_TAG, stdout);
    if (*display_data.total_down_time)
        puts(display_data.total_down_time);
    else
        puts(DSP_UNKNOWN_DISPLAY_DATA);

    fputs(DSP_TOTAL_DOWN_SIZE_DISPLAY_TAG, stdout);
    if (*display_data.total_down_size)
        puts(display_data.total_down_size);
    else
        puts(DSP_UNKNOWN_DISPLAY_DATA);

    fputs(DSP_AVERAGE_DOWN_RATE_DISPLAY_TAG, stdout);
    if (*display_data.average_down_rate)
        puts(display_data.average_down_rate);
    else
        puts(DSP_UNKNOWN_DISPLAY_DATA);

    fputs(DSP_PEAK_DOWN_RATE_DISPLAY_TAG, stdout);
    if (*display_data.peak_down_rate)
        puts(display_data.peak_down_rate);
    else
        puts(DSP_UNKNOWN_DISPLAY_DATA);

    fputs(DSP_LOWEST_DOWN_RATE_DISPLAY_TAG, stdout);
    if (*display_data.lowest_down_rate)
        puts(display_data.lowest_down_rate);
    else
        puts(DSP_UNKNOWN_DISPLAY_DATA);
}

static void dsp_perform(void)
{
    FILE *fp;
    CURL *cp;
    CURLcode c_status;

    fp = NULL;
    cp = NULL;

    if (!dsp_setup_curl(&cp, &fp))
        exit(EXIT_FAILURE);

    start_time = time(NULL);
    c_status = curl_easy_perform(cp);
    end_time = time(NULL);

    if (fp)
        fclose(fp);

    if (c_status != CURLE_OK) {
        dsp_print_error(curl_easy_strerror(c_status));
        curl_easy_cleanup(cp);
        exit(EXIT_FAILURE);
    }

  curl_easy_cleanup(cp);
  dsp_fill_display_data();
  dsp_show_display_data();
}

static void dsp_cleanup(void)
{
    if (temp_file_path) {
        if (!DSP_DELETE_FILE(temp_file_path))
            dsp_print_error("failed to delete temporary download file "
                    "`%s' (%s)", temp_file_path, strerror(errno));
        free(temp_file_path);
    }
}

int main(int argc, char **argv)
{
    atexit(dsp_cleanup);
    dsp_parse_options(argv);
    dsp_perform();
    exit(EXIT_SUCCESS);
    return 0; /* for compilers */
}


/*
 * dsp (Download SPeed) - Command line tool for testing download speed.
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

#ifndef __DSP_H__
#define __DSP_H__

#define DSP_DEFAULT_PROGRAM_NAME "dsp"
#define DSP_VERSION "1.0.2"

/* user agent string for libcurl */
#define DSP_USER_AGENT \
  DSP_DEFAULT_PROGRAM_NAME " (Download SPeed tester)/" DSP_VERSION

/* character constants for short command line options */
#define DSP_OPTION_SHORT_HELP0_C '?'
#define DSP_OPTION_SHORT_BIT_C 'b'
#define DSP_OPTION_SHORT_BYTE_C 'B'
#define DSP_OPTION_SHORT_HELP1_C 'h'
#define DSP_OPTION_SHORT_BINARY_C 'i'
#define DSP_OPTION_SHORT_LARGE_C 'L'
#define DSP_OPTION_SHORT_METRIC_C 'm'
#define DSP_OPTION_SHORT_MEDIUM_C 'M'
#define DSP_OPTION_SHORT_SMALL_C 'S'
#define DSP_OPTION_SHORT_URL_C 'u'
#define DSP_OPTION_SHORT_VERSION_C 'v'

/* string constants for short command line options */
#define DSP_OPTION_SHORT_HELP0_S "?"
#define DSP_OPTION_SHORT_BIT_S "b"
#define DSP_OPTION_SHORT_BYTE_S "B"
#define DSP_OPTION_SHORT_HELP1_S "h"
#define DSP_OPTION_SHORT_BINARY_S "i"
#define DSP_OPTION_SHORT_LARGE_S "L"
#define DSP_OPTION_SHORT_METRIC_S "m"
#define DSP_OPTION_SHORT_MEDIUM_S "M"
#define DSP_OPTION_SHORT_SMALL_S "S"
#define DSP_OPTION_SHORT_URL_S "u"
#define DSP_OPTION_SHORT_VERSION_S "v"

/* string constants for long command line options */
#define DSP_OPTION_LONG_BINARY "binary"
#define DSP_OPTION_LONG_BIT "bit"
#define DSP_OPTION_LONG_BYTE "byte"
#define DSP_OPTION_LONG_HELP "help"
#define DSP_OPTION_LONG_LARGE "large"
#define DSP_OPTION_LONG_MEDIUM "medium"
#define DSP_OPTION_LONG_METRIC "metric"
#define DSP_OPTION_LONG_SMALL "small"
#define DSP_OPTION_LONG_URL "url"
#define DSP_OPTION_LONG_VERSION "version"

#define DSP_BYTE_SYMBOL "B"
#define DSP_BIT_SYMBOL "bit"

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
#define DSP_TIME_BUFFER_SIZE 64
#define DSP_SIZE_BUFFER_SIZE 16
#define DSP_SPEED_BUFFER_SIZE 20

/* constants for dealing with time stuff */
#define DSP_SECONDS_IN_DAY 86400
#define DSP_SECONDS_IN_HOUR 3600
#define DSP_SECONDS_IN_MINUTE 60
#define DSP_MOD_VALUE_FOR_SECONDS 60

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

/* use a custom boolean type so there's no
   need to depend on the system's libc boolean type */
/* {{{ */
typedef unsigned char dsp_boolean_t;

#define DSP_FALSE ((dsp_boolean_t) 0)
#define DSP_TRUE ((dsp_boolean_t) 1)
/* }}} */


/* custom byte type for when dealing
   specifically with values pertaining to bytes */
/* {{{ */
typedef unsigned long dsp_byte_t;

/* initialization value */
#define DSP_ZERO_BYTES ((dsp_byte_t) 0lu)

/* units defined by the International Electrotechnical Commission (IEC) */
#define DSP_BINARY_KIBI ((dsp_byte_t) 1024LU)
#define DSP_BINARY_MEBI ((dsp_byte_t) 1048576LU)
#define DSP_BINARY_GIBI ((dsp_byte_t) 1073741824LU)
#define DSP_BINARY_TEBI ((dsp_byte_t) 1099511627776LU)

/* units defined by the International System of Units (SI) */
#define DSP_METRIC_KILO ((dsp_byte_t) 1000LU)
#define DSP_METRIC_MEGA ((dsp_byte_t) 1000000LU)
#define DSP_METRIC_GIGA ((dsp_byte_t) 1000000000LU)
#define DSP_METRIC_TERA ((dsp_byte_t) 1000000000000LU)
/* }}} */

/* tags for final output */
#define DSP_TOTAL_DOWN_TIME_DISPLAY_TAG "Total d/l time:   "
#define DSP_TOTAL_DOWN_SIZE_DISPLAY_TAG "Total d/l size:   "
#define DSP_AVERAGE_DOWN_RATE_DISPLAY_TAG "Average d/l rate: "
#define DSP_PEAK_DOWN_RATE_DISPLAY_TAG "Peak d/l rate:    "
#define DSP_LOWEST_DOWN_RATE_DISPLAY_TAG "Lowest d/l rate:  "

/* show this if the result data was not set for some reason */
#define DSP_UNKNOWN_DISPLAY_DATA "(unknown)"

/* a structure to hold all the final
   output strings containing the test results */
typedef struct
{
  char total_down_time[DSP_TIME_BUFFER_SIZE];
  char total_down_size[DSP_SIZE_BUFFER_SIZE];
  char average_down_rate[DSP_SPEED_BUFFER_SIZE];
  char peak_down_rate[DSP_SPEED_BUFFER_SIZE];
  char lowest_down_rate[DSP_SPEED_BUFFER_SIZE];
} dsp_display_data_t;

#endif /* __DSP_H__ */


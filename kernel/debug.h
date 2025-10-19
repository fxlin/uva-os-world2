/*
   print to tracebuffer support.
 
  Usage
  
   In compiler option or makefile: 
   
   set CONFIG_KAGE_GLOBAL_DEBUG_LEVEL=XX, where XX can be 10--50
   
	To supress a C file's debug messages, do: 
   // as the 1st line of a C file
   #define K2_DEBUG_INFO   // or K2_NO_DEBUG, K2_DEBUG_VERBOSE see below for other options
   // ..  later
   #include "debug.h"
 
    07/01/2015 -- change the func names (easier to use)
 */

#ifndef  __K2_TRACEBUFFER__
#define __K2_TRACEBUFFER__


#ifndef CONFIG_KAGE_GLOBAL_DEBUG_LEVEL
#define CONFIG_KAGE_GLOBAL_DEBUG_LEVEL 30 // default
#endif

#define print_to_tracebuffer printf

/* colorful output support. minicom -c on */
#define _k2clr_red   "\033[0;31m"        /* 0 -> normal ;  31 -> red */
#define _k2clr_cyan  "\033[1;36m"        /* 1 -> bold ;  36 -> cyan */
#define _k2clr_green "\033[0;32m"        /* 4 -> underline ;  32 -> green */
#define _k2clr_blue  "\033[1;34m"        /* 9 -> strike ;  34 -> blue */

#define _k2clr_black  "\033[0;30m"
#define _k2clr_brown  "\033[0;33m"
#define _k2clr_magenta  "\033[0;35m"
#define _k2clr_gray  "\033[0;37m"
#define _k2clr_u_gray  "\033[4;37m"   /* 4 -> underline, gray */

#define _k2clr_none   "\033[0m"        /* to flush the previous property */

/* example of prefix in output msgs */
#ifdef CONFIG_CPU_V7
#define K2_PRINT_TAG _k2clr_blue "{k}" _k2clr_none
#elif defined CONFIG_CPU_V7M
#define K2_PRINT_TAG "{m3}"
#else
#define K2_PRINT_TAG ""
#endif

#ifdef K2_DEBUG_VERBOSE
#define K2_LOCAL_DEBUG_LEVEL 10
#endif

#ifdef K2_DEBUG_NORMAL
#define K2_LOCAL_DEBUG_LEVEL 20
#endif

#ifdef K2_DEBUG_INFO
#define K2_LOCAL_DEBUG_LEVEL 30
#endif

#ifdef K2_DEBUG_WARN
#define K2_LOCAL_DEBUG_LEVEL 40
#endif

#ifdef K2_NO_DEBUG
#define K2_LOCAL_DEBUG_LEVEL 50
#endif

#ifndef K2_LOCAL_DEBUG_LEVEL
/* default local lv, if the source file says nothing.
 * By default, we show "V" (and more important) messages */
#define K2_LOCAL_DEBUG_LEVEL 20
#endif

/* global or local debug level, we select the higher one (i.e. fewer msgs)
 * (so that msgs are effectively suppressed) */
#if CONFIG_KAGE_GLOBAL_DEBUG_LEVEL > K2_LOCAL_DEBUG_LEVEL
#define K2_ACTUAL_DEBUG_LEVEL CONFIG_KAGE_GLOBAL_DEBUG_LEVEL
#else
#define K2_ACTUAL_DEBUG_LEVEL K2_LOCAL_DEBUG_LEVEL
#endif

/*

   Full format, including source path, which sometimes is long.
   Plus, there seems no reliable way to trim the path.

   #define k2_print(fmt, arg...) \
    print_to_tracebuffer(K2_PRINT_TAG "%s %d %s: " fmt, __FILE__, __LINE__, __func__, ## arg)

    Note: numbers should be consistent with arm/common/KConfig
*/

#if K2_ACTUAL_DEBUG_LEVEL <= 10
#define D(fmt, arg...) print_to_tracebuffer(fmt, ##arg)
#else
#define D(x...)
#endif

/* V conflicts with boost */
#if K2_ACTUAL_DEBUG_LEVEL <= 20
#define V(fmt, arg...) \
  print_to_tracebuffer(K2_PRINT_TAG  "%s:%d " fmt _k2clr_none "\r\n", __FILE__, __LINE__, ## arg)
#else
#define V(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 30
#define I(fmt, arg...) \
  print_to_tracebuffer(K2_PRINT_TAG _k2clr_green "%s:%d " fmt _k2clr_none "\r\n", __FILE__, __LINE__, ## arg)
#else
#define I(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 40
#define W(fmt, arg...) \
  print_to_tracebuffer(K2_PRINT_TAG _k2clr_brown "%s:%d " fmt _k2clr_none "\r\n", __FILE__, __LINE__, ## arg)
#else
#define W(fmt, arg...)
#endif

#if K2_ACTUAL_DEBUG_LEVEL <= 50
#define E(fmt, arg...) \
  print_to_tracebuffer(K2_PRINT_TAG _k2clr_red "%s:%d " fmt _k2clr_none "\r\n", __FILE__, __LINE__, ## arg)
#else
#error "not implemented or wrong debug level"
#endif

/* When specified global NO_DEBUG, if no local flag, fake one.
 * helps to suppress info msgs by print_to_tracebuffer() and guarded with K2_NO_DEBUG.
 * eg see fault.c */
#if (CONFIG_KAGE_GLOBAL_DEBUG_LEVEL == 50) && !defined(K2_NO_DEBUG)
#define K2_NO_DEBUG
#endif


/* print configuration macro values
 *
 * http://stackoverflow.com/questions/1164652/printing-name-and-value-of-a-macro
 *
 * NB: "you are willing to put up with the fact that SOMESTRING=SOMESTRING
 * indicates that SOMESTRING has not been defined.."
 * */
#define fxl_str(x)   #x
#define fxl_show_define(x) printf("%s %40s %10s %s\r\n", _k2clr_green, #x, fxl_str(x), _k2clr_none);
#define fxl_show_undefine(x) printf("%s %40s %10s %s\r\n", _k2clr_gray, #x, "undefined", _k2clr_none);

/*convert int number to G/M/K... */
#define int_val(x) \
({ \
	unsigned long v; 								\
	if (x < 1024)					\
		v = x;							\
	else if (x < 1024 * 1024)	\
		v = x / 1024;	\
	else if (x < 1024 * 1024 * 1024)	\
		v = x / (1024 * 1024);	\
	else	\
		v = x / (1024 * 1024 * 1024);	\
	v;	\
})

#define int_postfix(x) \
({ \
	const char *v; 								\
	if (x < 1024)					\
		v = "";							\
	else if (x < 1024 * 1024)	\
		v = "K";	\
	else if (x < 1024 * 1024 * 1024)	\
		v = "M";	\
	else	\
		v = "G";	\
	v;	\
})

// printf.c
void debug_hexdump (const void *pStart, unsigned nBytes); 
void assertion_failed (const char *pExpr, const char *pFile, unsigned nLine);

#endif // __K2_TRACEBUFFER__
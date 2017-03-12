#ifndef _DEBUG_H
#define _DEBUG_H
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>
 
//#define NOLOG

#ifndef NOLOG
#define DBG(args ...) \
	print_error( __FILE__, (char*)__func__, __LINE__, ##args )
#else
#define DBG(args ...) 

#endif
 
#define DBG(args ...) \
	print_error( __FILE__, (char*)__func__, __LINE__, ##args )
#define MSG	printf
void print_error(char* file, char* function, int line, const char *fmt, ...);
void debug_term_on();
void debug_term_off();
void debug_file_on();
void debug_file_off();
void debug_set_dir(char* str);
int mkdir_recursive( char* path );
#endif //_DEBUG_H


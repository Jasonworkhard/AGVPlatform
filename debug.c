/**
 *  debug.c
 *  hpflow debugger.
 *  Copyright (C) 2009  hp
 *  by lu 
 * use :DBG("like printf...");
**/

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include "debug.h"

static int dbg_term = 0, dbg_file = 0, log_day = 0;
static FILE* fp_log = NULL;
static char dir[128]={0,}, filename[160];
static void init_file_path();

void print_error(char* file, char* function, int line, const char *fmt, ...)
{
	va_list args;
	char printbuf[512];
	int i;
	if( !dbg_term && !dbg_file )
	{
		printf("%s(%d): function DBG can't used ! because debug log disenable!\n",function, line);
		return;
	}
	va_start(args, fmt);
	i=vsprintf( printbuf, fmt, args );
	printbuf[i] = 0;
	va_end(args);
	if( dbg_term ){
		printf("%s(%d): %s", function, line, printbuf);
	}
	if( dbg_file ){
		time_t t = time( NULL );
		struct tm* tm1 = localtime(&t);
		if( !tm1 ) return;
		if( tm1->tm_mday != log_day ){
			init_file_path();
		}
		char tmp[22];
		strftime( tmp, 22, "%F %X", tm1 );
		fprintf( fp_log, "%s [%s]%s(%d): %s", tmp, file, function, line, printbuf);
		fflush( fp_log );
	}
}

/**
@即时在终端打印调试信息
**/
void debug_term_on()
{
	dbg_term = 1;
}

/**
@不在终端打印调试信息
**/
void debug_term_off()
{
	dbg_term = 0;
}

void init_file_path()
{
	char tmp[64];
	time_t t = time( NULL );
	struct tm* tm1 = localtime(&t);
	if( !tm1 ){
		perror("log.c init_file_path: ERROR GETTING SYSTEM TIME.");
	}
	log_day = tm1->tm_mday;
	strftime( tmp, 64, "/%Y-%m-%d.txt", tm1 );
	if( access( dir, 0 )!=0 ){
		mkdir_recursive( dir );
	}
	strcpy( filename, dir );
	strcat( filename, tmp );
	if( fp_log )
		fclose( fp_log );
	fp_log = fopen( filename, "aw" );
}

void debug_file_on()
{
	if( dbg_file )
		return;
	init_file_path();
	dbg_file = 1;
}

void debug_file_off()
{
	if( !dbg_file )
		return;
	dbg_file = 0;
	if( fp_log )
		fclose( fp_log );
}

void debug_set_dir(char* str)
{
	strcpy( dir, str );
}

int mkdir_recursive( char* path )
{
	char *p;
	if( access( path, 0 ) == 0 )
		return 0;
	for( p=path; *p; p++ ){
		if( p>path && *p == '/' ){
			*p = 0;
			if( access( path, 0 ) != 0 ){
				if( mkdir( path, S_IRWXU ) != 0 )
					return -1;
			}
			*p = '/';
		}
	}
	return mkdir( path, S_IRWXU );
}


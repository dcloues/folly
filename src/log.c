#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

static FILE *logfile = NULL;

void hlog_init(char *filename)
{
	logfile = fopen(filename, "a+");
	if (logfile == NULL)
	{
		perror("Unable to open log file for writing");
	}
}

void hlog_shutdown()
{
	if (logfile != NULL)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

void hlog(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(logfile ? logfile : stderr, fmt, args);
	va_end(args);
}


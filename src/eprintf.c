/* Copyright (C) 1999 Lucent Technologies */
/* Excerpted from 'The Practice of Programming' */
/* by Brian W. Kernighan and Rob Pike */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "chord/eprintf.h"
#include "chord/logger/clog.h"

void log_strerror(logger_ctx_t *l, int level, const char *fmt, va_list args)
{
	StartLogTo(l, level);
	
	vfprintf(l->fp, fmt, args);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
		fprintf(l->fp, " %s", strerror(errno));
	
	EndLogTo(l);
}

/* eprintf: print error message and exit */
void eprintf_impl(logger_ctx_t *l, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_strerror(l, FATAL, fmt, args);
	va_end(args);

	exit(2); /* conventional value for failed execution */
}

/* weprintf: print warning message */
void weprintf_impl(logger_ctx_t *l, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_strerror(l, WARN, fmt, args);
	va_end(args);
}

/* emalloc: malloc and report if error */
void *emalloc(size_t n)
{
	void *p = malloc(n);
	if (p == NULL)
		eprintf("malloc of %u bytes failed:", n);
	return p;
}

/* erealloc: realloc and report if error */
void *erealloc(void *vp, size_t n)
{
	void *p = realloc(vp, n);
	if (p == NULL)
		eprintf("realloc of %u bytes failed:", n);
	return p;
}

/* ecalloc: calloc and report if error */
void *ecalloc(size_t n, size_t w)
{
	void *p = calloc(n, w);
	if (p == NULL)
		eprintf("calloc of %u x %u bytes failed:", n, w);
	return p;
}

/* estrdup: duplicate a string, report if error */
char *estrdup(char *s)
{
	char *t = (char *) malloc(strlen(s)+1);
	if (t == NULL)
		eprintf("estrdup(\"%.20s\") failed:", s);
	strcpy(t, s);
	return t;
}

#ifndef __APPLE__
static const char *progname;

/* setprogname: set name of program */
void setprogname(const char *name)
{
	progname = name;
}
#endif

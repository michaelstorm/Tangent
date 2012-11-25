/* Copyright (C) 1999 Lucent Technologies */
/* Excerpted from 'The Practice of Programming' */
/* by Brian W. Kernighan and Rob Pike */

#ifndef EPRINTF_H
#define EPRINTF_H

#include <sysexits.h>
#include "logger/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

void eprintf_impl(logger_ctx_t *l, const char *fmt, ...);
void weprintf_impl(logger_ctx_t *l, const char *fmt, ...);

char *estrdup(char *);
void *emalloc(size_t);
void *erealloc(void *, size_t);
void *ecalloc(size_t, size_t);
void setprogname(const char *);
const char*	getprogname(void);

#define eprintf(fmt, ...)  eprintf_impl (clog_file_logger(), fmt, ##__VA_ARGS__)
#define weprintf(fmt, ...) weprintf_impl(clog_file_logger(), fmt, ##__VA_ARGS__)

#ifdef CCURED
#pragma ccuredvararg("eprintf", printf(1))
#pragma ccuredvararg("weprintf", printf(1))
#endif

#ifdef __cplusplus
}
#endif

#endif
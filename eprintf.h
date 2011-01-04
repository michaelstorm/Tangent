/* Copyright (C) 1999 Lucent Technologies */
/* Excerpted from 'The Practice of Programming' */
/* by Brian W. Kernighan and Rob Pike */

/* eprintf.h: error wrapper functions */
void eprintf(char *, ...);
void weprintf(char *, ...);
#ifdef CCURED
#pragma ccuredvararg("eprintf", printf(1))
#pragma ccuredvararg("weprintf", printf(1))
#endif
char *estrdup(char *);
void *emalloc(size_t);
void *erealloc(void *, size_t);
void *ecalloc(size_t, size_t);
void setprogname(const char *);
const char*	getprogname(void);

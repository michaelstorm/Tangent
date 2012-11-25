#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libio.h>
#include <errno.h>
#include <time.h>
#include "chord/logger/hashmap.h"
#include "chord/logger/logger.h"
#include "chord/logger/color.h"

#define BASENAME(file) strrchr(file, '/') ? strrchr(file, '/') + 1 : file

static map_t loggers;

int clog_default_log_level = 0;

static int default_level_colors[] = {
	FG_PURPLE|MOD_INTENSE_FG, FG_CYAN|MOD_INTENSE_FG, FG_GREEN|MOD_INTENSE_FG, FG_YELLOW|MOD_INTENSE_FG, FG_RED|MOD_INTENSE_FG, FG_WHITE|BG_RED
};

struct timespec start_time;

void clog_init()
{
	loggers = hashmap_new();
	
	char *min_str = getenv("CLOG_LOG_LEVEL");
	if (min_str != NULL) {
		if (strlen(min_str) == 0) {
			// do nothing
		}
		else if (strcmp(min_str, "TRACE") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_TRACE;
		else if (strcmp(min_str, "DEBUG") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_DEBUG;
		else if (strcmp(min_str, "INFO") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_INFO;
		else if (strcmp(min_str, "WARN") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_WARN;
		else if (strcmp(min_str, "ERROR") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_ERROR;
		else if (strcmp(min_str, "FATAL") == 0)
			clog_default_log_level = CLOG_LOG_LEVEL_FATAL;
		else {
			errno = 0;
			long value = strtol(min_str, NULL, 10);
			if (errno == 0)
				clog_default_log_level = value;
			else
				Log(ERROR, "Environment variable CLOG_LOG_LEVEL must be unset, empty, an integer, or a log level name");
		}
	}
	
	start_time.tv_sec = 0;
	start_time.tv_nsec = 0;
}

int start_file_msg(logger_ctx_t *l, const char *file, int line, const char *func, int level)
{	
	FILE *fp = (FILE *)l->data;

	int color = level >= CLOG_LOG_LEVEL_TRACE && level <= CLOG_LOG_LEVEL_FATAL ? default_level_colors[level] : 0;
	start_color(fp, color|ATTR_BOLD);

#ifdef LOG_BRACKET_LEADER
	char leader[level+1];
	memset(leader, '>', level);
	leader[level] = '\0';
	fprintf(fp, "%s> ", leader);
#endif
	
	struct timespec current_time;

	if (start_time.tv_sec == 0 && start_time.tv_nsec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &start_time);
		current_time.tv_sec = start_time.tv_sec;
		current_time.tv_nsec = start_time.tv_nsec;
	}
	else
		clock_gettime(CLOCK_MONOTONIC, &current_time);
	
	struct timespec diff_time;
	diff_time.tv_sec = current_time.tv_sec - start_time.tv_sec;
	diff_time.tv_nsec = current_time.tv_nsec - start_time.tv_nsec;
	if (diff_time.tv_nsec < 0) {
		diff_time.tv_sec--;
		diff_time.tv_nsec += 1000000000L;
	}
	diff_time.tv_nsec = (diff_time.tv_nsec - (diff_time.tv_nsec % 1000000L)) / 1000000L;
	
	int ret = fprintf(fp, "[%3lu.%.3lu] {%s} (%s) %s@%d: ", diff_time.tv_sec, diff_time.tv_nsec, l->name, func, BASENAME(file), line);
	
	default_color(fp);
	start_color(fp, color);
	
	return ret;
}

ssize_t write_file(FILE *file, const char *buf, size_t size)
{
	return fwrite(buf, 1, size, file);
}

int end_file_msg(logger_ctx_t *l)
{
	FILE *fp = (FILE *)l->data;
	default_color(fp);
	int ret = fwrite("\n", 1, 1, fp) != 1;
	ret |= fflush(fp);
	return ret;
}

ssize_t logger_call_write(logger_ctx_t *l, const char *buf, size_t size)
{
	if (l->log_partial)
		return l->write(l->data, buf, size);
	else
		return size;
}

int logger_ctx_init(logger_ctx_t *l, const char *name, int min_level, void *data, start_msg_func start_msg, printf_func printf, write_func write, end_msg_func end_msg)
{
	int name_len = strlen(name);
	l->name = malloc(name_len+1);
	strcpy(l->name, name);
	l->name[name_len] = '\0';
	
	l->min_level = min_level;
	
	l->data = data;
	l->start_msg = start_msg;
	l->printf = printf;
	l->write = write;
	l->end_msg = end_msg;
	
	l->log_partial = 0;
	
	cookie_io_functions_t funcs = {
		.write = (cookie_write_function_t *)logger_call_write,
		.read = NULL,
		.seek = NULL,
		.close = NULL
	};
	
	if ((l->fp = fopencookie(l, "w+", funcs)) == NULL)
		return 1;
	
	return 0;
}

logger_ctx_t *logger_ctx_new(const char *name, int min_level, void *data, start_msg_func start_msg, printf_func printf, write_func write, end_msg_func end_msg)
{
	logger_ctx_t *l = malloc(sizeof(logger_ctx_t));
	if (logger_ctx_init(l, name, min_level, data, start_msg, printf, write, end_msg))
		return NULL;
	
	return l;
}

logger_ctx_t *logger_ctx_new_file(const char *name, int min_level, FILE *file)
{
	logger_ctx_t *l = malloc(sizeof(logger_ctx_t));
	if (logger_ctx_init(l, name, min_level, file, (start_msg_func)start_file_msg, (printf_func)vfprintf, (write_func)write_file, (end_msg_func)end_file_msg))
		return NULL;
	
	return l;
}

void logger_ctx_close(logger_ctx_t *l)
{
}

void logger_ctx_free(logger_ctx_t *l)
{
	logger_ctx_close(l);
	free(l);
}

int clog_add_logger(logger_ctx_t *l_new)
{
	return hashmap_put(loggers, (char *)l_new->name, l_new);
}

logger_ctx_t *clog_get_logger(const char *name)
{
	logger_ctx_t *l;
	if (hashmap_get(loggers, (char *)name, (any_t *)&l)) {
		l = logger_ctx_new_file(name, clog_default_log_level, stdout);
		if (clog_add_logger(l))
			return NULL;
	}
	return l;
}

logger_ctx_t *clog_get_logger_for_file(const char *file)
{
	file = BASENAME(file);
	int len = strchr(file, '.') - file;
	
	char file_base[len+1];
	strncpy(file_base, file, len);
	file_base[len] = '\0';
	
	return clog_get_logger(file_base);
}

void clog_start_log(logger_ctx_t *l, const char *file, int line, const char *func, int level)
{
	if (l == NULL)
		l = clog_get_logger_for_file(file);
	
	if (level >= l->min_level) {
		l->log_partial = 1;
		
		if (l->start_msg != NULL)
			l->start_msg(l, file, line, func, level);
	}
}

#define PARTIAL_LOG_IMPL(l, last_arg, fmt) \
	if (l->log_partial) { \
		fflush(l->fp); /* make sure writes are not concatenated out of order */ \
		\
		va_list args; \
		va_start(args, last_arg); \
		l->printf(l->data, fmt, args); \
		va_end(args); \
	}

void clog_partial_log(logger_ctx_t *l, const char *file, const char *fmt, ...)
{
	if (l == NULL)
		l = clog_get_logger_for_file(file);
	
	PARTIAL_LOG_IMPL(l, fmt, fmt)
}

void clog_end_log(logger_ctx_t *l, const char *file)
{
	if (l == NULL)
		l = clog_get_logger_for_file(file);
	
	/*
	 * Flush whether or not log_partial is true, to ensure that data buffered
	 * on the file descriptor is not flushed at the wrong time (which would be
	 * when log_partial becomes true again).
	 */
	fflush(l->fp);
	
	if (l->log_partial) {
		if (l->end_msg != NULL)
			l->end_msg(l);
		
		// fflush() calls write(), which checks log_partial, so make
		// sure to clear the flag only after that's done
		l->log_partial = 0;
	}
}

void clog_start_log_as(const char *name, const char *file, int line, const char *func, int level)
{
	logger_ctx_t *l = clog_get_logger(name);
	clog_start_log(l, file, line, func, level);
}

void clog_partial_log_as(const char *name, const char *fmt, ...)
{
	logger_ctx_t *l = clog_get_logger(name);
	PARTIAL_LOG_IMPL(l, fmt, fmt)
}

void clog_end_log_as(const char *name)
{
	logger_ctx_t *l = clog_get_logger(name);
	clog_end_log(l, NULL);
}

void vlog(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, va_list args)
{
	clog_start_log(l, file, line, func, level);
	l->printf(l->data, fmt, args);
	clog_end_log(l, NULL);
}

#define CALL_VLOG \
{ \
	va_list args; \
	va_start(args, fmt); \
	vlog(l, file, line, func, level, fmt, args); \
	va_end(args); \
}

void clog_log_as(const char *name, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	logger_ctx_t *l = clog_get_logger(name);
	
	if (level >= l->min_level)
		CALL_VLOG
}

void clog_log(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	if (l == NULL)
		l = clog_get_logger_for_file(file);
	
	if (level >= l->min_level)
		CALL_VLOG
}
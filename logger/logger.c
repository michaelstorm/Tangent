#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libio.h>
#include <errno.h>
#include "hashmap.h"
#include "logger.h"

#define BASENAME(file) strrchr(file, '/') ? strrchr(file, '/') + 1 : file

static map_t loggers;

int logger_default_level = INT_MIN;

#define LOGGER_WRITE(l, buf) l->write(l->data, buf, strlen(buf))

void logger_init()
{
	loggers = hashmap_new();
	
	char *min_str = getenv("LOG_LEVEL");
	if (min_str != NULL) {
		if (strlen(min_str) == 0) {
			// do nothing
		}
		else if (strcmp(min_str, "TRACE") == 0)
			logger_default_level = LOG_LEVEL_TRACE;
		else if (strcmp(min_str, "DEBUG") == 0)
			logger_default_level = LOG_LEVEL_DEBUG;
		else if (strcmp(min_str, "INFO") == 0)
			logger_default_level = LOG_LEVEL_INFO;
		else if (strcmp(min_str, "WARN") == 0)
			logger_default_level = LOG_LEVEL_WARN;
		else if (strcmp(min_str, "ERROR") == 0)
			logger_default_level = LOG_LEVEL_ERROR;
		else if (strcmp(min_str, "FATAL") == 0)
			logger_default_level = LOG_LEVEL_FATAL;
		else {
			errno = 0;
			long value = strtol(min_str, NULL, 10);
			if (errno == 0)
				logger_default_level = value;
			else
				Log(ERROR, "Environment variable LOG_LEVEL must be unset or empty, an integer, or a log level name");
		}
	}
}

int start_file_msg(logger_ctx_t *l, const char *file, int line, const char *func, int level)
{	
	char leader[level+1];
	memset(leader, '>', level);
	leader[level] = '\0';
	
	return fprintf((FILE *)l->data, "%s> [%s] (%s) %s@%d: ", leader, l->name, func, BASENAME(file), line);
}

ssize_t write_file(FILE *file, const char *buf, size_t size)
{
	return fwrite(buf, 1, size, file);
}

int end_file_msg(logger_ctx_t *l)
{
	int ret = fwrite("\n", 1, 1, (FILE *)l->data) != 1;
	ret |= fflush((FILE *)l->data);
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

int add_logger(logger_ctx_t *l_new)
{
	return hashmap_put(loggers, (char *)l_new->name, l_new);
}

logger_ctx_t *get_logger(const char *name)
{
	logger_ctx_t *l;
	if (hashmap_get(loggers, (char *)name, (any_t *)&l)) {
		l = logger_ctx_new_file(name, logger_default_level, stdout);
		if (add_logger(l))
			return NULL;
	}
	return l;
}

logger_ctx_t *get_logger_for_file(const char *file)
{
	file = BASENAME(file);
	int len = strchr(file, '.') - file;
	
	char file_base[len+1];
	strncpy(file_base, file, len);
	file_base[len] = '\0';
	
	return get_logger(file_base);
}

void StartLog_impl(logger_ctx_t *l, const char *file, int line, const char *func, int level)
{
	if (l == NULL)
		l = get_logger_for_file(file);
	
	if (level >= l->min_level) {
		l->log_partial = 1;
		
		if (l->start_msg != NULL)
			l->start_msg(l, file, line, func, level);
	}
}

#define PARTIAL_LOG_IMPL(l, last_arg, fmt) \
	if (l->log_partial) { \
		va_list args; \
		va_start(args, last_arg); \
		l->printf(l->data, fmt, args); \
		va_end(args); \
	}

void PartialLog_impl(logger_ctx_t *l, const char *file, const char *fmt, ...)
{
	if (l == NULL)
		l = get_logger_for_file(file);
	
	PARTIAL_LOG_IMPL(l, fmt, fmt)
}

void EndLog_impl(logger_ctx_t *l, const char *file)
{
	if (l == NULL)
		l = get_logger_for_file(file);
	
	if (l->log_partial) {
		l->log_partial = 0;
		
		fflush(l->fp);
		if (l->end_msg != NULL)
			l->end_msg(l);
	}
}

void StartLogAs_impl(const char *name, const char *file, int line, const char *func, int level)
{
	logger_ctx_t *l = get_logger(name);
	StartLog_impl(l, file, line, func, level);
}

void PartialLogAs_impl(const char *name, const char *fmt, ...)
{
	logger_ctx_t *l = get_logger(name);
	PARTIAL_LOG_IMPL(l, fmt, fmt)
}

void EndLogAs_impl(const char *name)
{
	logger_ctx_t *l = get_logger(name);
	EndLog_impl(l, NULL);
}

void vlog(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, va_list args)
{
	StartLog_impl(l, file, line, func, level);
	l->printf(l->data, fmt, args);
	EndLog_impl(l, NULL);
}

#define CALL_VLOG \
{ \
	va_list args; \
	va_start(args, fmt); \
	vlog(l, file, line, func, level, fmt, args); \
	va_end(args); \
}

void LogAs_impl(const char *name, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	logger_ctx_t *l = get_logger(name);
	
	if (level >= l->min_level)
		CALL_VLOG
}

void Log_impl(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	if (l == NULL)
		l = get_logger_for_file(file);
	
	if (level >= l->min_level)
		CALL_VLOG
}
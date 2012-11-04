#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libio.h>
#include "hashmap.h"
#include "logger.h"

#define BASENAME(file) strrchr(file, '/') ? strrchr(file, '/') + 1 : file

static map_t loggers;

int logger_default_level = INT_MIN;

void logger_init()
{
	loggers = hashmap_new();
}

ssize_t write_file(void *data, const char *buf, size_t len)
{
	return fwrite(buf, 1, len, (FILE *)data) != len;
}

int end_file_msg(void *data)
{
	int ret = fwrite("\n", 1, 1, (FILE *)data) != 1;
	fflush((FILE *)data);
	return ret;
}

int logger_ctx_init(logger_ctx_t *l, const char *name, int min_level, void *data, start_msg_func start_msg, write_func write, end_msg_func end_msg)
{
	int name_len = strlen(name);
	l->name = malloc(name_len+1);
	strcpy(l->name, name);
	l->name[name_len] = '\0';
	
	l->min_level = min_level;

	l->data = data;
	l->start_msg = start_msg;
	l->write = write;
	l->end_msg = end_msg;
	
	cookie_io_functions_t funcs = {
		.write = l->write,
		.read = NULL,
		.seek = NULL,
		.close = NULL
	};
	
	if ((l->fp = fopencookie(l->data, "w+", funcs)) == NULL)
		return 1;
	
	return 0;
}

logger_ctx_t *logger_ctx_new(const char *name, int min_level, void *data, start_msg_func start_msg, write_func write, end_msg_func end_msg)
{
	logger_ctx_t *l = malloc(sizeof(logger_ctx_t));
	if (logger_ctx_init(l, name, min_level, data, start_msg, write, end_msg))
		return NULL;
	
	return l;
}

logger_ctx_t *logger_ctx_new_file(const char *name, int min_level, FILE *file)
{
	logger_ctx_t *l = malloc(sizeof(logger_ctx_t));
	if (logger_ctx_init(l, name, min_level, file, NULL, write_file, end_file_msg))
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

#define PRINT_VAR_BUF(buf, last_arg, fmt) \
{ \
	va_list args; \
	va_start(args, last_arg); \
	int formatted_len = vsnprintf(NULL, 0, fmt, args) + 1; \
	va_end(args); \
	\
	buf = malloc(formatted_len); \
	\
	va_start(args, last_arg); \
	vsprintf(buf, fmt, args); \
	va_end(args); \
}

char *print_var_buf(const char *fmt, ...)
{
	char *buf;
	PRINT_VAR_BUF(buf, fmt, fmt)
	return buf;
}

static const char *std_header_fmt = "[%s] (%s) %s$%d: ";

void StartLog_impl(logger_ctx_t *l, const char *file, int line, const char *func, int level)
{
	if (l->start_msg != NULL)
		l->start_msg(l->data, level);
	
	int i;
	for (i = 0; i < level; i++)
		l->write(l->data, " ", 1);
	l->write(l->data, ">", 1);
	
	char *header = print_var_buf(std_header_fmt, l->name, func, BASENAME(file), line);
	
	l->write(l->data, header, strlen(header));
	free(header);
}

#define PARTIAL_LOG_IMPL(l, last_arg, fmt) \
{ \
	char *buf; \
	PRINT_VAR_BUF(buf, fmt, fmt) \
	\
	l->write(l->data, buf, strlen(buf)); \
	free(buf); \
}

void PartialLog_impl(logger_ctx_t *l, const char *fmt, ...)
{
	PARTIAL_LOG_IMPL(l, fmt, fmt)
}

void EndLog_impl(logger_ctx_t *l)
{
	if (l->end_msg != NULL)
		l->end_msg(l->data);
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
	EndLog_impl(l);
}

void vlog(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *buf)
{
	StartLog_impl(l, file, line, func, level);
	l->write(l->data, buf, strlen(buf));
	EndLog_impl(l);
}

void LogAs_impl(const char *name, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	logger_ctx_t *l = get_logger(name);
	
	if (level >= l->min_level) {
		char *buf;
		PRINT_VAR_BUF(buf, fmt, fmt)
		
		va_list args;
		va_start(args, fmt);
		vlog(l, file, line, func, level, buf);
		va_end(args);
		
		free(buf);
	}
}

void Log_impl(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, ...)
{
	if (l == NULL) {
		file = BASENAME(file);
		int len = strchr(file, '.') - file;
		
		char file_base[len+1];
		strncpy(file_base, file, len);
		file_base[len] = '\0';
		
		l = get_logger(file_base);
	}
	
	if (level >= l->min_level) {
		char *buf;
		PRINT_VAR_BUF(buf, fmt, fmt)
		
		va_list args;
		va_start(args, fmt);
		vlog(l, file, line, func, level, buf);
		va_end(args);
		
		free(buf);
	}
}
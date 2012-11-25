#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	CLOG_LOG_LEVEL_TRACE = 0,
	CLOG_LOG_LEVEL_DEBUG,
	CLOG_LOG_LEVEL_INFO,
	CLOG_LOG_LEVEL_WARN,
	CLOG_LOG_LEVEL_ERROR,
	CLOG_LOG_LEVEL_FATAL
} log_level_t;

#ifndef DISABLE_SHORT_CLOG_LOG_LEVEL_NAMES
typedef enum
{
	TRACE = 0,
	DEBUG,
	INFO,
	WARN,
	ERROR,
	FATAL
} short_log_level_t;
#endif

struct logger_ctx_t;
typedef struct logger_ctx_t logger_ctx_t;
typedef int (*start_msg_func) (logger_ctx_t *l, const char *file, int line, const char *func, int level);
typedef int (*printf_func)    (void *data, const char *fmt, va_list args);
typedef ssize_t (*write_func) (void *data, const char *buf, size_t size);
typedef int (*end_msg_func)   (logger_ctx_t *l);

struct logger_ctx_t
{
	char *name;
	int min_level;
	
	void *data;
	
	start_msg_func start_msg;
	printf_func printf;
	write_func write;
	end_msg_func end_msg;
	
	FILE *fp;
	int log_partial;
};

extern int clog_default_log_level;

void clog_init();
int clog_add_logger(logger_ctx_t *l_new);
logger_ctx_t *clog_get_logger(const char *name);
logger_ctx_t *clog_get_logger_for_file(const char *file);

void clog_log(logger_ctx_t *l, const char *file, int line, const char *func, int level, const char *fmt, ...);
void clog_log_as(const char *name, const char *file, int line, const char *func, int level, const char *fmt, ...);

void clog_start_log(logger_ctx_t *l, const char *file, int line, const char *func, int level);
void clog_partial_log(logger_ctx_t *l, const char *file, const char *fmt, ...);
void clog_end_log(logger_ctx_t *l, const char *file);

void clog_start_log_as(const char *name, const char *file, int line, const char *func, int level);
void clog_partial_log_as(const char *name, const char *fmt, ...);
void clog_end_log_as(const char *name);

#define clog_file_logger() clog_get_logger_for_file(__FILE__)

#define Log(level, fmt, ...) clog_log(NULL, __FILE__, __LINE__, __func__, level,                fmt, ##__VA_ARGS__)
#define LogTrace(fmt, ...)   clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LogDebug(fmt, ...)   clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LogInfo(fmt, ...)    clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LogWarn(fmt, ...)    clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LogError(fmt, ...)   clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LogFatal(fmt, ...)   clog_log(NULL, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define StartLog(level)      { clog_start_log(NULL, __FILE__, __LINE__, __func__, level)
#define PartialLog(fmt, ...)   clog_partial_log(NULL, __FILE__, fmt, ##__VA_ARGS__)
#define EndLog()             } clog_end_log(NULL, __FILE__)

#define LogTo(l_ctx, level, fmt, ...) clog_log(l_ctx, __FILE__, __LINE__, __func__, level,                fmt, ##__VA_ARGS__)
#define LogTraceTo(l_ctx, fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LogDebugTo(l_ctx, fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LogInfoTo(l_ctx,  fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LogWarnTo(l_ctx,  fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LogErrorTo(l_ctx, fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LogFatalTo(l_ctx, fmt, ...)   clog_log(l_ctx, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define StartLogTo(l_ctx, level)      { clog_start_log(l_ctx, __FILE__, __LINE__, __func__, level)
#define PartialLogTo(l_ctx, fmt, ...)   clog_partial_log(l_ctx, __FILE__, fmt, ##__VA_ARGS__)
#define EndLogTo(l_ctx)               } clog_end_log(l_ctx, __FILE__)

#define LogAs(name, level, fmt, ...) clog_log_as(name, __FILE__, __LINE__, __func__, level,                fmt, ##__VA_ARGS__)
#define LogTraceAs(name, fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LogDebugAs(name, fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LogInfoAs(name,  fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LogWarnAs(name,  fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LogErrorAs(name, fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LogFatalAs(name, fmt, ...)   clog_log_as(name, __FILE__, __LINE__, __func__, CLOG_LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define StartLogAs(name, level)      { clog_start_log_as(name, __FILE__, __LINE__, __func__, level)
#define PartialLogAs(name, fmt, ...)   clog_partial_log_as(name, fmt, ##__VA_ARGS__)
#define EndLogAs(name)               } clog_end_log_as(name)

#define Die(ret_code, fmt, ...)          { LogFatal(fmt, ##__VA_ARGS__);          LogFatal("Exiting with code " #ret_code);          exit(ret_code); }
#define DieTo(l_ctx, ret_code, fmt, ...) { LogFatalTo(l_ctx, fmt, ##__VA_ARGS__); LogFatalTo(l_ctx, "Exiting with code " #ret_code); exit(ret_code); }
#define DieAs(name, ret_code, fmt, ...)  { LogFatalAs(name, fmt,  ##__VA_ARGS__); LogFatalAs(name, "Exiting with code " #ret_code);  exit(ret_code); }

#ifndef CLOG_DISABLE_SHORT_LOGGER_NAMES
	#define Trace   LogTrace
	#define Debug   LogDebug
	#define Info    LogInfo
	#define Warn    LogWarn
	#define Error   LogError
	#define Fatal   LogFatal

	#define TraceTo LogTraceTo
	#define DebugTo LogDebugTo
	#define InfoTo  LogInfoTo
	#define WarnTo  LogWarnTo
	#define ErrorTo LogErrorTo
	#define FatalTo LogFatalTo

	#define TraceAs LogTraceAs
	#define DebugAs LogDebugAs
	#define InfoAs  LogInfoAs
	#define WarnAs  LogWarnAs
	#define ErrorAs LogErrorAs
	#define FatalAs LogFatalAs
#endif

#ifdef __cplusplus
}
#endif

#endif
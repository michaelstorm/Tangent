#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "chord/logger/clog.h"
#include "chord/logger/color.h"
#include "chord/logger/file.h"
#include "clog_internal.h"

static int default_level_colors[] = {
	FG_PURPLE|MOD_INTENSE_FG, FG_CYAN|MOD_INTENSE_FG, FG_GREEN|MOD_INTENSE_FG, FG_YELLOW|MOD_INTENSE_FG, FG_RED|MOD_INTENSE_FG, FG_WHITE|BG_RED
};

logger_ctx_t *logger_ctx_new_file(const char *name, int min_level, FILE *file)
{
	logger_ctx_t *l = malloc(sizeof(logger_ctx_t));
	if (logger_ctx_init(l, name, min_level, file, (start_msg_func)start_file_msg, (printf_func)vfprintf, (write_func)write_file, (end_msg_func)end_file_msg))
		return NULL;
	
	return l;
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

	int ret;
	struct timespec diff_time;
	if (clog_time_offset(&diff_time))
		ret = fprintf(fp, "[<unkwn>] {%s} (%s) %s@%d: ", l->name, func, BASENAME(file), line);
	else
		ret = fprintf(fp, "[%3lu.%.3lu] {%s} (%s) %s@%d: ", diff_time.tv_sec, diff_time.tv_nsec, l->name, func, BASENAME(file), line);
	
	if (ret > 0)
		fprintf(fp, "%*s", -70 + ret, "");

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

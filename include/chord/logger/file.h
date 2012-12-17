#ifndef CLOG_FILE_H
#define CLOG_FILE_H

#include <string.h>
#include "chord/logger/clog.h"

logger_ctx_t *logger_ctx_new_file(const char *name, int min_level, FILE *file);
		 int  start_file_msg(logger_ctx_t *l, const char *file, int line, const char *func, int level);
	 ssize_t  write_file(FILE *file, const char *buf, size_t size);
         int  end_file_msg(logger_ctx_t *l);
     ssize_t  logger_call_write(logger_ctx_t *l, const char *buf, size_t size);

#endif
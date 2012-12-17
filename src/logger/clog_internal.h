#ifndef CLOG_INTERNAL_H
#define CLOG_INTERNAL_H

#include <string.h>

#define BASENAME(file) strrchr(file, '/') ? strrchr(file, '/') + 1 : file

#endif

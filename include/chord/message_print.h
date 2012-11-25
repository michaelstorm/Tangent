#ifndef MESSAGE_PRINT_H
#define MESSAGE_PRINT_H

#include "chord/logger/clog.h"
#include "messages.pb-c.h"

#ifdef __cplusplus
extern "C" {
#endif

void protobuf_c_message_print(const ProtobufCMessage *message, FILE *out);

#define LogMessageTo(l_ctx, level, header, msg) \
{ \
	StartLogTo(l_ctx, level); \
	PartialLogTo(l_ctx, "%s\n", header); \
	protobuf_c_message_print(msg, l_ctx->fp); \
	EndLogTo(l_ctx); \
}

#define LogMessage(level, header, msg)   LogMessageTo(clog_get_logger_for_file(__FILE__), level, header, msg)
#define LogMessageAs(level, header, msg) LogMessageTo(clog_get_logger(name), level, header, msg)

#ifdef __cplusplus
}
#endif

#endif
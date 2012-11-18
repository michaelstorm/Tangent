#ifndef MESSAGE_PRINT_H
#define MESSAGE_PRINT_H

#include "messages.pb-c.h"

#ifdef __cplusplus
extern "C" {
#endif

void protobuf_c_message_print(const ProtobufCMessage *message, FILE *out);

#ifdef __cplusplus
}
#endif

#endif
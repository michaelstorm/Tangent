#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "chord.h"
#include "ctype.h"
#include "dispatcher.h"
#include "messages.pb-c.h"

static uchar msg_buf[BUFSIZE];

int pack_header(uchar *buf, int type, uchar *payload, int n)
{
	Header header = HEADER__INIT;
	header.type = type;
	header.payload.len = n;
	header.payload.data = payload;
	return header__pack(&header, buf);
}

int pack_addr_discover(uchar *buf, uchar *ticket, int ticket_len)
{
	AddrDiscover msg = ADDR_DISCOVER__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	int n = addr_discover__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_ADDR_DISCOVER, msg_buf, n);
}

int pack_addr_discover_reply(uchar *buf, uchar *ticket, int ticket_len, in6_addr *addr)
{
	AddrDiscoverReply msg = ADDR_DISCOVER_REPLY__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	int n = addr_discover_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_ADDR_DISCOVER_REPL, msg_buf, n);
}

int pack_data(uchar *buf, int last, uchar ttl, chordID *id, ushort len,
			  const uchar *data)
{
	Data msg = DATA__INIT;
	msg.id.len = CHORD_ID_LEN;
	msg.id.data = id->x;

	msg.ttl = ttl;
	msg.has_ttl = 1;
	msg.last = last;
	msg.has_last = 1;

	msg.data.len = len;
	msg.data.data = (uint8_t *)data;

	int n = data__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_DATA, msg_buf, n);
}

int pack_fs(uchar *buf, uchar *ticket, int ticket_len, uchar ttl, in6_addr *addr, ushort port)
{
	FindSuccessor msg = FIND_SUCCESSOR__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.ttl = ttl;
	msg.has_ttl = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = find_successor__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_FS, msg_buf, n);
}

int pack_fs_reply(uchar *buf, uchar *ticket, int ticket_len, in6_addr *addr,
				  ushort port)
{
	FindSuccessorReply msg = FIND_SUCCESSOR_REPLY__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = find_successor_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_FS_REPL, msg_buf, n);
}

int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	Stabilize msg = STABILIZE__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = stabilize__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_STAB, msg_buf, n);
}

int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port)
{
	StabilizeReply msg = STABILIZE_REPLY__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;

	msg.port = port;
	int n = stabilize_reply__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_STAB_REPL, msg_buf, n);
}

int pack_notify(uchar *buf)
{
	Notify msg = NOTIFY__INIT;
	int n = notify__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_NOTIFY, msg_buf, n);
}

int pack_ping(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Ping msg = PING__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	int n = ping__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_PING, msg_buf, n);
}

int pack_pong(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Pong msg = PONG__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.time = time;
	int n = pong__pack(&msg, msg_buf);
	return pack_header(buf, CHORD_PONG, msg_buf, n);
}

static inline size_t
sizeof_elt_in_repeated_array (ProtobufCType type)
{
  switch (type)
	{
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
	case PROTOBUF_C_TYPE_FIXED32:
	case PROTOBUF_C_TYPE_FLOAT:
	case PROTOBUF_C_TYPE_ENUM:
	  return 4;
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
	case PROTOBUF_C_TYPE_FIXED64:
	case PROTOBUF_C_TYPE_DOUBLE:
	  return 8;
	case PROTOBUF_C_TYPE_BOOL:
	  return sizeof (protobuf_c_boolean);
	case PROTOBUF_C_TYPE_STRING:
	case PROTOBUF_C_TYPE_MESSAGE:
	  return sizeof (void *);
	case PROTOBUF_C_TYPE_BYTES:
	  return sizeof (ProtobufCBinaryData);
	}
  PROTOBUF_C_ASSERT_NOT_REACHED ();
  return 0;
}

static void message_body_print(const ProtobufCMessage *message, FILE *out,
							   int tabs);

static void
required_field_print(const ProtobufCFieldDescriptor *field,
					 const void *member,
					 FILE *out, int tabs)
{
  switch (field->type)
	{
	case PROTOBUF_C_TYPE_SFIXED32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_INT32:
	  fprintf(out, "%d", *(const int32_t *)member); break;
	case PROTOBUF_C_TYPE_FIXED32:
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_ENUM:
	  fprintf(out, "%u", *(const uint32_t *)member); break;
	case PROTOBUF_C_TYPE_SFIXED64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_INT64:
	  fprintf(out, "%lld", *(const int64_t *)member); break;
	case PROTOBUF_C_TYPE_FIXED64:
	case PROTOBUF_C_TYPE_UINT64:
	  fprintf(out, "%llu", *(const uint64_t *)member); break;
	case PROTOBUF_C_TYPE_FLOAT:
	  fprintf(out, "%f", *(const float *)member); break;
	case PROTOBUF_C_TYPE_DOUBLE:
	  fprintf(out, "%lf", *(const double *)member); break;
	case PROTOBUF_C_TYPE_BOOL:
	  fprintf(out, "%s", *(const protobuf_c_boolean *)member
							? "true" : "false");
	  break;
	case PROTOBUF_C_TYPE_STRING:
	  fprintf(out, "[%d] \"%s\"", strlen(*(char * const *)member),
			  *(char * const *)member);
	  break;
	case PROTOBUF_C_TYPE_BYTES:
	  {
		const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) member);
		fprintf(out, "[%d] ", bd->len);
		int i;
		for (i = 0; i < bd->len; i++)
			fprintf(out, "%02x ", bd->data[i]);
		break;
	  }
	//case PROTOBUF_C_TYPE_GROUP:          // NOT SUPPORTED
	case PROTOBUF_C_TYPE_MESSAGE:
	  fprintf(out, "\n");
		message_body_print(*(ProtobufCMessage * const *)member, out, tabs);
		break;
	}
}

static void
optional_field_print(const ProtobufCFieldDescriptor *field,
					 const protobuf_c_boolean *has,
					 const void *member,
					 FILE *out, int tabs)
{
  if (field->type == PROTOBUF_C_TYPE_MESSAGE
   || field->type == PROTOBUF_C_TYPE_STRING)
	{
	  const void *ptr = * (const void * const *) member;
	  if (ptr == NULL) {
		  fprintf(out, "<none>");
		  return;
	  }
	  else if (ptr == field->default_value) {
		  fprintf(out, "<default> (");
		  required_field_print(field, field->default_value, out, tabs);
		  fprintf(out, ")");
		  return;
	  }
	}
  else if (!*has) {
	  fprintf(out, "<none>");
	  return;
  }
  return required_field_print(field, member, out, tabs);
}

static void print_tabs(FILE *out, int tabs)
{
	int i;
	for (i = 0; i < tabs; i++)
		fprintf(out, "\t");
}

static void print_nl(FILE *out, int tabs)
{
	fprintf(out, "\n");
	print_tabs(out, tabs);
}

static void
repeated_field_print(const ProtobufCFieldDescriptor *field,
					 size_t count,
					 const void *member,
					 FILE *out, int tabs)
{
	fprintf(out, "[%d] ", count);
	if (count > 0) {
		if (field->type == PROTOBUF_C_TYPE_MESSAGE) {
			char *array = * (char * const *)member;
			unsigned siz = sizeof_elt_in_repeated_array(field->type);
			unsigned i;
			for (i = 0; i < count; i++) {
				required_field_print(field, array, out, tabs);
				array += siz;
			}
		}
		else {
			fprintf(out, "{");

			char *array = * (char * const *)member;
			unsigned siz = sizeof_elt_in_repeated_array(field->type);
			unsigned i;
			for (i = 0; i < count; i++) {
				required_field_print(field, array, out, tabs);
				array += siz;
				if (i < count-1)
					fprintf(out, ", ");
			}

			fprintf(out, "}");
		}
	}
}

static void
unknown_field_print(const ProtobufCMessageUnknownField *field,
					FILE *out)
{
	switch (field->wire_type) {
	case PROTOBUF_C_WIRE_TYPE_VARINT:
		fprintf(out, "<varint>"); break;
	case PROTOBUF_C_WIRE_TYPE_64BIT:
		fprintf(out, "<64bit>"); break;
	case PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED:
		fprintf(out, "<length_prefixed>"); break;
	case PROTOBUF_C_WIRE_TYPE_START_GROUP:
		fprintf(out, "<start_group>"); break;
	case PROTOBUF_C_WIRE_TYPE_END_GROUP:
		fprintf(out, "<end_group>"); break;
	case PROTOBUF_C_WIRE_TYPE_32BIT:
		fprintf(out, "<32bit>"); break;
	}
	fprintf(out, " [%d]", field->len);
}

static void type_name_print(const ProtobufCFieldDescriptor *field, FILE *out)
{
	switch (field->type) {
	case PROTOBUF_C_TYPE_INT32:
		fprintf(stderr, "int32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT32:
		fprintf(stderr, "sint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED32:
		fprintf(stderr, "sfixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_INT64:
		fprintf(stderr, "int64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT64:
		fprintf(stderr, "sint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED64:
		fprintf(stderr, "sfixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT32:
		fprintf(stderr, "uint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED32:
		fprintf(stderr, "fixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT64:
		fprintf(stderr, "uint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED64:
		fprintf(stderr, "fixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FLOAT:
		fprintf(stderr, "float %s", field->name); break;
	case PROTOBUF_C_TYPE_DOUBLE:
		fprintf(stderr, "double %s", field->name); break;
	case PROTOBUF_C_TYPE_BOOL:
		fprintf(stderr, "bool %s", field->name); break;
	case PROTOBUF_C_TYPE_ENUM:
		{
			const ProtobufCEnumDescriptor *desc = field->descriptor;
			fprintf(stderr, "%s %s", desc->name, field->name);
			break;
		}
	case PROTOBUF_C_TYPE_STRING:
		fprintf(stderr, "string %s", field->name); break;
	case PROTOBUF_C_TYPE_BYTES:
		fprintf(stderr, "bytes %s", field->name); break;
	case PROTOBUF_C_TYPE_MESSAGE:
	{
		const ProtobufCMessageDescriptor *desc = field->descriptor;
		fprintf(stderr, "%s %s", desc->name, field->name);
		break;
	}
	}
}

static void message_body_print(const ProtobufCMessage *message, FILE *out,
							   int tabs)
{
	print_tabs(out, tabs);
	fprintf(out, "{");
	tabs++;
  unsigned i;
  for (i = 0; i < message->descriptor->n_fields; i++)
	{
	  const ProtobufCFieldDescriptor *field = message->descriptor->fields + i;
	  const void *member = ((const char *) message) + field->offset;
	  const void *qmember = ((const char *) message) + field->quantifier_offset;

	  print_nl(out, tabs);
	  fprintf(out, "%d: ", field->id);
	  if (field->label == PROTOBUF_C_LABEL_REQUIRED) {
		  fprintf(out, "required ");
		  type_name_print(field, out);
		  fprintf(out, " = ");
		  required_field_print(field, member, out, tabs);
	  }
	  else if (field->label == PROTOBUF_C_LABEL_OPTIONAL) {
		  fprintf(out, "optional ");
		  type_name_print(field, out);
		  fprintf(out, " = ");
		  optional_field_print(field, qmember, member, out, tabs);
	  }
	  else {
		  fprintf(out, "repeated ");
		  type_name_print(field, out);
		  fprintf(out, " = ");
		  repeated_field_print(field, *(const size_t *)qmember, member, out,
							   tabs);
	  }
	}
  for (i = 0; i < message->n_unknown_fields; i++) {
	  fprintf(out, "unknown ");
	  unknown_field_print(&message->unknown_fields[i], out);
	  print_nl(out, tabs);
  }
  tabs--;
  print_nl(out, tabs);
  fprintf(out, "}");
}

void protobuf_c_message_print(const ProtobufCMessage *message, FILE *out)
{
	fprintf(out, "message %s\n", message->descriptor->name);
	message_body_print(message, out, 0);
	fprintf(out, "\n");
}

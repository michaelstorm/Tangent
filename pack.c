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

int pack_header(uchar *buf, int version, int type, const ProtobufCMessage *msg)
{
	Header header = HEADER__INIT;
	header.version = version;
	header.has_version = 1;

	header.type = type;
	header.payload.len = protobuf_c_message_pack(msg, msg_buf);
	header.payload.data = msg_buf;
	return header__pack(&header, buf);
}

int pack_addr_discover(uchar *buf, uchar *ticket, int ticket_len)
{
	AddrDiscover msg = ADDR_DISCOVER__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	return pack_chord_header(buf, CHORD_ADDR_DISCOVER, &msg);
}

int pack_addr_discover_reply(uchar *buf, uchar *ticket, int ticket_len,
							 in6_addr *addr)
{
	AddrDiscoverReply msg = ADDR_DISCOVER_REPLY__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;

	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	return pack_chord_header(buf, CHORD_ADDR_DISCOVER_REPLY, &msg);
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
	return pack_chord_header(buf, CHORD_DATA, &msg);
}

int pack_fs(uchar *buf, uchar *ticket, int ticket_len, uchar ttl,
			in6_addr *addr, ushort port)
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
	return pack_chord_header(buf, CHORD_FS, &msg);
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
	return pack_chord_header(buf, CHORD_FS_REPLY, &msg);
}

int pack_stab(uchar *buf, in6_addr *addr, ushort port)
{
	Stabilize msg = STABILIZE__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	return pack_chord_header(buf, CHORD_STAB, &msg);
}

int pack_stab_reply(uchar *buf, in6_addr *addr, ushort port)
{
	StabilizeReply msg = STABILIZE_REPLY__INIT;
	msg.addr.len = 16;
	msg.addr.data = addr->s6_addr;
	msg.port = port;
	return pack_chord_header(buf, CHORD_STAB_REPLY, &msg);
}

int pack_notify(uchar *buf)
{
	Notify msg = NOTIFY__INIT;
	return pack_chord_header(buf, CHORD_NOTIFY, &msg);
}

int pack_ping(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Ping msg = PING__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	msg.time = time;
	return pack_chord_header(buf, CHORD_PING, &msg);
}

int pack_pong(uchar *buf, uchar *ticket, int ticket_len, ulong time)
{
	Pong msg = PONG__INIT;
	msg.ticket.len = ticket_len;
	msg.ticket.data = ticket;
	msg.has_ticket = 1;
	msg.time = time;
	return pack_chord_header(buf, CHORD_PONG, &msg);
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

static void message_body_print(const ProtobufCMessage *message, LinkedString *out,
							   int tabs);

static void
required_field_print(const ProtobufCFieldDescriptor *field,
					 const void *member,
					 LinkedString *out, int tabs)
{
  switch (field->type)
	{
	case PROTOBUF_C_TYPE_SFIXED32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_INT32:
	  lstr_add(out, "%d", *(const int32_t *)member); break;
	case PROTOBUF_C_TYPE_FIXED32:
	case PROTOBUF_C_TYPE_UINT32:
	  lstr_add(out, "%u", *(const uint32_t *)member); break;
	case PROTOBUF_C_TYPE_SFIXED64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_INT64:
	  lstr_add(out, "%lld", *(const int64_t *)member); break;
	case PROTOBUF_C_TYPE_FIXED64:
	case PROTOBUF_C_TYPE_UINT64:
	  lstr_add(out, "%llu", *(const uint64_t *)member); break;
	case PROTOBUF_C_TYPE_FLOAT:
	  lstr_add(out, "%f", *(const float *)member); break;
	case PROTOBUF_C_TYPE_DOUBLE:
	  lstr_add(out, "%lf", *(const double *)member); break;
	case PROTOBUF_C_TYPE_BOOL:
	  lstr_add(out, "%s", *(const protobuf_c_boolean *)member
							? "true" : "false");
	  break;
	case PROTOBUF_C_TYPE_STRING:
	  lstr_add(out, "[%d] \"%s\"", strlen(*(char * const *)member),
			  *(char * const *)member);
	  break;
	case PROTOBUF_C_TYPE_BYTES:
	  {
		const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) member);
		lstr_add(out, "[%d] ", bd->len);
		int i;
		for (i = 0; i < bd->len; i++)
			lstr_add(out, "%02x ", bd->data[i]);

		int printable = 1;
		for (i = 0; i < bd->len; i++) {
			if (!isprint(bd->data[i])) {
				printable = 0;
				break;
			}
		}

		if (printable && bd->len > 0) {
			lstr_add(out, "(\"");
			for (i = 0; i < bd->len; i++)
				lstr_add(out, "%c", bd->data[i]);
			lstr_add(out, "\")");
		}
		break;
	  }
	//case PROTOBUF_C_TYPE_GROUP:          // NOT SUPPORTED
	case PROTOBUF_C_TYPE_MESSAGE:
	  lstr_add(out, "\n");
		message_body_print(*(ProtobufCMessage * const *)member, out, tabs);
		break;
	case PROTOBUF_C_TYPE_ENUM:
	{
		uint32_t value = *(const uint32_t *)member;
		const ProtobufCEnumDescriptor *desc = field->descriptor;
		lstr_add(out, "%u", value);
		if (value < desc->n_values)
			lstr_add(out, " (%s)", desc->values[value].name);
		break;
	}
	}
}

static void
optional_field_print(const ProtobufCFieldDescriptor *field,
					 const protobuf_c_boolean *has,
					 const void *member,
					 LinkedString *out, int tabs)
{
  if (field->type == PROTOBUF_C_TYPE_MESSAGE
   || field->type == PROTOBUF_C_TYPE_STRING)
	{
	  const void *ptr = * (const void * const *) member;
	  if (ptr == NULL) {
		  lstr_add(out, "<none>");
		  return;
	  }
	  else if (ptr == field->default_value) {
		  lstr_add(out, "<default> (");
		  required_field_print(field, field->default_value, out, tabs);
		  lstr_add(out, ")");
		  return;
	  }
	}
  else if (!*has) {
	  lstr_add(out, "<none>");
	  return;
  }
  required_field_print(field, member, out, tabs);
}

static void print_tabs(LinkedString *out, int tabs)
{
	int i;
	for (i = 0; i < tabs; i++)
		lstr_add(out, "\t");
}

static void print_nl(LinkedString *out, int tabs)
{
	lstr_add(out, "\n");
	print_tabs(out, tabs);
}

static void
repeated_field_print(const ProtobufCFieldDescriptor *field,
					 size_t count,
					 const void *member,
					 LinkedString *out, int tabs)
{
	lstr_add(out, "[%d] ", count);
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
			lstr_add(out, "{");

			char *array = * (char * const *)member;
			unsigned siz = sizeof_elt_in_repeated_array(field->type);
			unsigned i;
			for (i = 0; i < count; i++) {
				required_field_print(field, array, out, tabs);
				array += siz;
				if (i < count-1)
					lstr_add(out, ", ");
			}

			lstr_add(out, "}");
		}
	}
}

static void
unknown_field_print(const ProtobufCMessageUnknownField *field,
					LinkedString *out)
{
	switch (field->wire_type) {
	case PROTOBUF_C_WIRE_TYPE_VARINT:
		lstr_add(out, "<varint>"); break;
	case PROTOBUF_C_WIRE_TYPE_64BIT:
		lstr_add(out, "<64bit>"); break;
	case PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED:
		lstr_add(out, "<length_prefixed>"); break;
	case PROTOBUF_C_WIRE_TYPE_START_GROUP:
		lstr_add(out, "<start_group>"); break;
	case PROTOBUF_C_WIRE_TYPE_END_GROUP:
		lstr_add(out, "<end_group>"); break;
	case PROTOBUF_C_WIRE_TYPE_32BIT:
		lstr_add(out, "<32bit>"); break;
	}
	lstr_add(out, " [%d]", field->len);
}

static void type_name_print(const ProtobufCFieldDescriptor *field, LinkedString *out)
{
	switch (field->type) {
	case PROTOBUF_C_TYPE_INT32:
		lstr_add(out, "int32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT32:
		lstr_add(out, "sint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED32:
		lstr_add(out, "sfixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_INT64:
		lstr_add(out, "int64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT64:
		lstr_add(out, "sint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED64:
		lstr_add(out, "sfixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT32:
		lstr_add(out, "uint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED32:
		lstr_add(out, "fixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT64:
		lstr_add(out, "uint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED64:
		lstr_add(out, "fixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FLOAT:
		lstr_add(out, "float %s", field->name); break;
	case PROTOBUF_C_TYPE_DOUBLE:
		lstr_add(out, "double %s", field->name); break;
	case PROTOBUF_C_TYPE_BOOL:
		lstr_add(out, "bool %s", field->name); break;
	case PROTOBUF_C_TYPE_ENUM:
	{
		const ProtobufCEnumDescriptor *desc = field->descriptor;
		lstr_add(out, "%s %s", desc->name, field->name);
		break;
	}
	case PROTOBUF_C_TYPE_STRING:
		lstr_add(out, "string %s", field->name); break;
	case PROTOBUF_C_TYPE_BYTES:
		lstr_add(out, "bytes %s", field->name); break;
	case PROTOBUF_C_TYPE_MESSAGE:
	{
		const ProtobufCMessageDescriptor *desc = field->descriptor;
		lstr_add(out, "%s %s", desc->name, field->name);
		break;
	}
	}
}

static void message_body_print(const ProtobufCMessage *message, LinkedString *out,
							   int tabs)
{
	print_tabs(out, tabs);
	lstr_add(out, "{ [%d]", message->descriptor->sizeof_message);
	tabs++;
  unsigned i;
  for (i = 0; i < message->descriptor->n_fields; i++)
	{
	  const ProtobufCFieldDescriptor *field = message->descriptor->fields + i;
	  const void *member = ((const char *) message) + field->offset;
	  const void *qmember = ((const char *) message) + field->quantifier_offset;

	  print_nl(out, tabs);
	  lstr_add(out, "%d: ", field->id);
	  if (field->label == PROTOBUF_C_LABEL_REQUIRED) {
		  lstr_add(out, "required ");
		  type_name_print(field, out);
		  lstr_add(out, " = ");
		  required_field_print(field, member, out, tabs);
	  }
	  else if (field->label == PROTOBUF_C_LABEL_OPTIONAL) {
		  lstr_add(out, "optional ");
		  type_name_print(field, out);
		  lstr_add(out, " = ");
		  optional_field_print(field, qmember, member, out, tabs);
	  }
	  else {
		  lstr_add(out, "repeated ");
		  type_name_print(field, out);
		  lstr_add(out, " = ");
		  repeated_field_print(field, *(const size_t *)qmember, member, out,
							   tabs);
	  }
	}
  for (i = 0; i < message->n_unknown_fields; i++) {
	  lstr_add(out, "unknown ");
	  unknown_field_print(&message->unknown_fields[i], out);
	  print_nl(out, tabs);
  }
  tabs--;
  print_nl(out, tabs);
  lstr_add(out, "}");
}

void protobuf_c_message_print(const ProtobufCMessage *message, LinkedString *out)
{
	lstr_add(out, "%s ", message->descriptor->name);
	message_body_print(message, out, 0);
	lstr_add(out, "\n");
}

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "chord/message_print.h"

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
	  fprintf(out, "%u", *(const uint32_t *)member); break;
	case PROTOBUF_C_TYPE_SFIXED64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_INT64:
	  fprintf(out, "%"PRId64, *(const int64_t *)member); break;
	case PROTOBUF_C_TYPE_FIXED64:
	case PROTOBUF_C_TYPE_UINT64:
	  fprintf(out, "%"PRIu64, *(const uint64_t *)member); break;
	case PROTOBUF_C_TYPE_FLOAT:
	  fprintf(out, "%f", *(const float *)member); break;
	case PROTOBUF_C_TYPE_DOUBLE:
	  fprintf(out, "%lf", *(const double *)member); break;
	case PROTOBUF_C_TYPE_BOOL:
	  fprintf(out, "%s", *(const protobuf_c_boolean *)member
							? "true" : "false");
	  break;
	case PROTOBUF_C_TYPE_STRING:
	  fprintf(out, "[%zu] \"%s\"", strlen(*(char * const *)member),
			  *(char * const *)member);
	  break;
	case PROTOBUF_C_TYPE_BYTES:
	  {
		const ProtobufCBinaryData * bd = ((const ProtobufCBinaryData*) member);
		fprintf(out, "[%zu] ", bd->len);
		int i;
		for (i = 0; i < bd->len; i++)
			fprintf(out, "%02x ", bd->data[i]);

		int printable = 1;
		for (i = 0; i < bd->len; i++) {
			if (!isprint(bd->data[i])) {
				printable = 0;
				break;
			}
		}

		if (printable && bd->len > 0) {
			fprintf(out, "(\"");
			for (i = 0; i < bd->len; i++)
				fprintf(out, "%c", bd->data[i]);
			fprintf(out, "\")");
		}
		break;
	  }
	//case PROTOBUF_C_TYPE_GROUP:          // NOT SUPPORTED
	case PROTOBUF_C_TYPE_MESSAGE:
	  fprintf(out, "\n");
		message_body_print(*(ProtobufCMessage * const *)member, out, tabs);
		break;
	case PROTOBUF_C_TYPE_ENUM:
	{
		int32_t value = *(const int32_t *) member;
		const ProtobufCEnumValue *evalue = protobuf_c_enum_descriptor_get_value(field->descriptor, value);
		fprintf(out, "%d (%s)", evalue->value, evalue->name);
		break;
	}
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
  required_field_print(field, member, out, tabs);
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
	fprintf(out, "[%zu] ", count);
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
	fprintf(out, " [%zu]", field->len);
}

static void type_name_print(const ProtobufCFieldDescriptor *field, FILE *out)
{
	switch (field->type) {
	case PROTOBUF_C_TYPE_INT32:
		fprintf(out, "int32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT32:
		fprintf(out, "sint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED32:
		fprintf(out, "sfixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_INT64:
		fprintf(out, "int64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SINT64:
		fprintf(out, "sint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_SFIXED64:
		fprintf(out, "sfixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT32:
		fprintf(out, "uint32 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED32:
		fprintf(out, "fixed32 %s", field->name); break;
	case PROTOBUF_C_TYPE_UINT64:
		fprintf(out, "uint64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FIXED64:
		fprintf(out, "fixed64 %s", field->name); break;
	case PROTOBUF_C_TYPE_FLOAT:
		fprintf(out, "float %s", field->name); break;
	case PROTOBUF_C_TYPE_DOUBLE:
		fprintf(out, "double %s", field->name); break;
	case PROTOBUF_C_TYPE_BOOL:
		fprintf(out, "bool %s", field->name); break;
	case PROTOBUF_C_TYPE_ENUM:
	{
		const ProtobufCEnumDescriptor *desc = field->descriptor;
		fprintf(out, "%s %s", desc->name, field->name);
		break;
	}
	case PROTOBUF_C_TYPE_STRING:
		fprintf(out, "string %s", field->name); break;
	case PROTOBUF_C_TYPE_BYTES:
		fprintf(out, "bytes %s", field->name); break;
	case PROTOBUF_C_TYPE_MESSAGE:
	{
		const ProtobufCMessageDescriptor *desc = field->descriptor;
		fprintf(out, "%s %s", desc->name, field->name);
		break;
	}
	}
}

static void message_body_print(const ProtobufCMessage *message, FILE *out,
							   int tabs)
{
	print_tabs(out, tabs);
	fprintf(out, "{ [%zu]", message->descriptor->sizeof_message);
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
	fprintf(out, "%s ", message->descriptor->name);
	message_body_print(message, out, 0);
}
#include <openssl/evp.h>
#include <string.h>
#include "chord/chord.h"
#include "chord/message_print.h"
#include "chord/util.h"

void vpack_hash(int debug, EVP_MD_CTX *ctx, const char *fmt, va_list args)
{
	char c;
	ushort s;
	ulong l;
	chordID *id;
	in6_addr *v6addr;
	
	for (; *fmt != '\0'; fmt++) {
		switch (*fmt) {
		case 'c':	 /* char */
			c = va_arg(args, int);
			EVP_DigestUpdate(ctx, &c, 1);

			if (debug)
				Trace("hashing char %d", (char)c);
			break;

		case 's':	 /* short */
			s = va_arg(args, int);
			EVP_DigestUpdate(ctx, &s, sizeof(ushort));

			if (debug)
				Trace("hashing short %d", (short)s);
			break;

		case 'l':	 /* long */
			l = va_arg(args, ulong);
			EVP_DigestUpdate(ctx, &l, sizeof(ulong));

			if (debug)
				Trace("hashing long %ld", l);
			break;

		case 'x':	 /* id */
			id = va_arg(args, chordID *);
			EVP_DigestUpdate(ctx, id->x, CHORD_ID_LEN);

			if (debug)
				Trace("hashing id %s", buf_to_hex(id->x, CHORD_ID_LEN));
			break;

		case '6':
			v6addr = va_arg(args, in6_addr *);
			EVP_DigestUpdate(ctx, v6addr->s6_addr, 16);

			if (debug)
				Trace("hashing address %s", v6addr_to_str(v6addr));
			break;

		default:
			Error("bad ticket type %c", *fmt);
			break;
		}
	}
}

void pack_hash(int debug, EVP_MD_CTX *ctx, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vpack_hash(debug, ctx, fmt, args);
	va_end(args);
}

void log_salt(int level, const uchar *salt, int salt_len)
{
	StartLog(level);
	PartialLog("hashing salt %s (%d)", buf_to_hex(salt, salt_len), salt_len);
	EndLog();
}

int pack_ticket_impl(const uchar *salt, int salt_len, int hash_len, const uchar *out, const char *args_str, const char *fmt, ...)
{
	va_list args;
	uchar md_value[EVP_MAX_MD_SIZE];
	uint32_t epoch_time = (uint32_t)time(NULL);

	// hash the unix epoch time together with the caller's arguments
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	log_salt(TRACE, salt, salt_len);
	EVP_DigestUpdate(&ctx, salt, salt_len);

	Trace("hashing time %" PRIu32, epoch_time);
	pack_hash(1, &ctx, "l", epoch_time);

	Trace("packing ticket with format \"%s\" and args \"%s\"", fmt, args_str);

	va_start(args, fmt);
	vpack_hash(1, &ctx, fmt, args);
	va_end(args);

	unsigned int len;
	EVP_DigestFinal_ex(&ctx, md_value, &len);
	EVP_MD_CTX_cleanup(&ctx);
	
	Trace("created hash %s", buf_to_hex(md_value, len));

	// pack the 32-bit epoch time and 32-bit hash into a buffer
	Ticket ticket = TICKET__INIT;
	ticket.time = epoch_time;
	ticket.hash.len = hash_len;
	ticket.hash.data = md_value;
	
	LogMessage(TRACE, "Packed ticket:", &ticket.base);
	
	return ticket__pack(&ticket, (uint8_t *)out);
}

int verify_ticket_impl(const uchar *salt, int salt_len, int hash_len,
					   const uchar *ticket_buf, int ticket_len, const char *args_str, const char *fmt, ...)
{
	va_list args;
	uchar md_value[EVP_MAX_MD_SIZE];

	// decrypt the ticket
	Ticket *ticket = ticket__unpack(NULL, ticket_len, ticket_buf);
	if (!ticket) {
		Warn("Ticket verification failed because the ticket could not be unpacked");
		goto fail;
	}
	if (ticket->hash.len != hash_len) {
		Warn("Ticket verification failed because ticket length %ul does not match expected length %ul", ticket->hash.len, hash_len);
		goto fail;
	}

	LogMessage(TRACE, "Verifying ticket:", &ticket->base);

	time_t current_time = time(NULL);
	if (ticket->time < current_time-TICKET_TIMEOUT) {
		Warn("Ticket failed due to timeout; ticket timestamp is %ul, current time is %ul, configured ticket timeout is %ul", ticket->time, current_time, TICKET_TIMEOUT);
		goto fail;
	}

	// hash together the time provided in the ticket with the data given in the
	// arguments (that were presumably in the packet received) to verify that
	// the remote host didn't modify our ticket
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	log_salt(TRACE, salt, salt_len);
	EVP_DigestUpdate(&ctx, salt, salt_len);

	Trace("hashing time %"PRIu32, ticket->time);
	pack_hash(1, &ctx, "l", ticket->time);
	
	Trace("verifying ticket with format \"%s\" and args \"%s\"", fmt, args_str);

	va_start(args, fmt);
	vpack_hash(1, &ctx, fmt, args);
	va_end(args);
	
	unsigned int len;
	EVP_DigestFinal_ex(&ctx, md_value, &len);
	EVP_MD_CTX_cleanup(&ctx);
	
	Trace("created hash %s", buf_to_hex(md_value, len));
	
	int ret = memcmp(md_value, ticket->hash.data, hash_len) == 0;
	if (!ret) {
		Debug("Expecting ticket hash: %s", buf_to_hex(md_value, hash_len));
		Debug("Message ticket hash:   %s", buf_to_hex(ticket->hash.data, hash_len));
	}
	
	ticket__free_unpacked(ticket, NULL);
	return ret;

fail:
	ticket__free_unpacked(ticket, NULL);
	return 0;
}

void get_data_id(chordID *id, const uchar *buf, int n)
{
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	EVP_DigestUpdate(&ctx, buf, n);

	unsigned int len;
	EVP_DigestFinal_ex(&ctx, id->x, &len);
	EVP_MD_CTX_cleanup(&ctx);
}

void get_address_id(chordID *id, in6_addr *addr, ushort port)
{
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	pack_hash(0, &ctx, "6s", addr, htons(port));

	unsigned int len;
	EVP_DigestFinal_ex(&ctx, id->x, &len);
	EVP_MD_CTX_cleanup(&ctx);
}

int verify_address_id(chordID *id, in6_addr *addr, ushort port)
{
	chordID correct_id;
	get_address_id(&correct_id, addr, port);
	return equals(&correct_id, id);
}

#include <openssl/blowfish.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>
#include "chord.h"

#undef C_DEBUG_ON

#ifdef C_DEBUG_ON
#define C_DEBUG(x) x
#else
#define C_DEBUG(x)
#endif

int vpack_hash(const EVP_MD *type, const uchar *out, const uchar *buf,
			   int buf_len, const char *fmt, va_list args)
{
	EVP_MD_CTX ctx;
	char c;
	ushort s;
	ulong l;
	chordID *id;
	in6_addr *v6addr;

	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, type, NULL);

	if (buf && buf_len) {
		EVP_DigestUpdate(&ctx, buf, buf_len);
#ifdef C_DEBUG_ON
		fprintf(stderr, "buf: ");
		int i;
		for (i = 0; i < buf_len; i++)
			fprintf(stderr, "%02x", buf[i]);
		fprintf(stderr, "\n");
#endif
	}

	for (; *fmt != '\0'; fmt++) {
		switch (*fmt) {
		case 'c':	 /* char */
			c = va_arg(args, int);
			EVP_DigestUpdate(&ctx, &c, 1);

			C_DEBUG(fprintf(stderr, "char: %02x\n", c));
			break;
		case 's':	 /* short */
			s = va_arg(args, int);
			EVP_DigestUpdate(&ctx, &s, sizeof(ushort));

			C_DEBUG(fprintf(stderr, "short: %hu\n", s));
			break;
		case 'l':	 /* long */
			l = va_arg(args, ulong);
			EVP_DigestUpdate(&ctx, &l, sizeof(ulong));

			C_DEBUG(fprintf(stderr, "long: %lu\n", l));
			break;
		case 'x':	 /* id */
			id = va_arg(args, chordID *);
			EVP_DigestUpdate(&ctx, id->x, CHORD_ID_LEN);

			C_DEBUG(fprintf(stderr, "chordID: "));
			C_DEBUG(print_id(stderr, id));
			C_DEBUG(fprintf(stderr, "\n"));
			break;
		case '6':
			v6addr = va_arg(args, in6_addr *);
			EVP_DigestUpdate(&ctx, v6addr->s6_addr, 16);

			C_DEBUG(fprintf(stderr, "addr: %s\n", v6addr_to_str(v6addr)));
			break;
		default:	 /* illegal type character */
			fprintf(stderr, "bad ticket type %c", *fmt);
			return 0;
		}
	}

	unsigned int len;
	EVP_DigestFinal_ex(&ctx, (uchar *)out, &len);
	EVP_MD_CTX_cleanup(&ctx);
	return len;
}

int pack_hash(const EVP_MD *type, const uchar *out, const uchar *buf,
			  int buf_len, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vpack_hash(type, out, buf, buf_len, fmt, args);
	va_end(args);
	return ret;
}

int pack_ticket(BF_KEY *key, const uchar *out, const char *fmt, ...)
{
	va_list args;
	uchar md_value[EVP_MAX_MD_SIZE];
	uint32_t epoch_time = (uint32_t)time(NULL);

#ifdef C_DEBUG_ON
	fprintf(stderr, "\ngenerating:\n");

	fprintf(stderr, "time: ");
	int i;
	for (i = 0; i < 4; i++)
		fprintf(stderr, "%02x", ((uchar *)&epoch_time)[i]);
	fprintf(stderr, " %u\n", epoch_time);
#endif

	// hash the unix epoch time together with the caller's arguments
	va_start(args, fmt);
	vpack_hash(EVP_sha1(), md_value, (uchar *)&epoch_time, sizeof(epoch_time),
			   fmt, args);
	va_end(args);

	// pack the 32-bit epoch time and 32-bit hash into a buffer
	struct {
		uint32_t time;
		uchar md[4];
	} __attribute__((__packed__)) ticket_value;

	ticket_value.time = epoch_time;
	memcpy(ticket_value.md, md_value, 4);

	// and encrypt it using our secret function, which happens to be a single-
	// block blowfish cipher (note that the EVP_CIPHER_CTX* functions still
	// generate extraneous padding, even if we turn it off, so we'll call the
	// blowfish encryption function manually)
	BF_ecb_encrypt((uchar *)&ticket_value, (uchar *)out, key, BF_ENCRYPT);

#ifdef C_DEBUG_ON
	fprintf(stderr, "ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		fprintf(stderr, "%02x", ticket_value[i]);
	fprintf(stderr, "\n");

	fprintf(stderr, "encrypted ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		fprintf(stderr, "%02x", out[i]);
	fprintf(stderr, "\n");
#endif

	return 1;
}

int verify_ticket(BF_KEY *key, const uchar *ticket_enc, const char *fmt, ...)
{
	va_list args;
	uchar ticket[TICKET_LEN];
	uchar md_value[EVP_MAX_MD_SIZE];
	uchar *ticket_md;
	uint32_t ticket_time;

	// decrypt the ticket
	BF_ecb_encrypt(ticket_enc, ticket, key, BF_DECRYPT);

#ifdef C_DEBUG_ON
	fprintf(stderr, "\nverifying:\n");

	fprintf(stderr, "encrypted ticket: ");
	int i;
	for (i = 0; i < TICKET_LEN; i++)
		fprintf(stderr, "%02x", ticket_enc[i]);
	fprintf(stderr, "\n");

	fprintf(stderr, "ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		fprintf(stderr, "%02x", ticket[i]);
	fprintf(stderr, "\n");
#endif

	struct {
		uint32_t time;
		uchar md[4];
	} __attribute__((__packed__)) *ticket_value = ticket;

	ticket_time = ticket_value->time;
	ticket_md = ticket_value->md;

	if (ticket_time < time(NULL)-TICKET_TIMEOUT)
		return 0;

	// hash together the time provided in the ticket with the data given in the
	// arguments (that were presumably in the packet received) to verify that
	// the remote host didn't modify our ticket
	va_start(args, fmt);
	vpack_hash(EVP_sha1(), md_value, (uchar *)&ticket_time, sizeof(ticket_time),
			   fmt, args);
	va_end(args);

#ifdef C_DEBUG_ON
	fprintf(stderr, "time: ");
	for (i = 0; i < 4; i++)
		fprintf(stderr, "%02x", ((uchar *)&ticket_time)[i]);
	fprintf(stderr, " %u\n", ticket_time);

	fprintf(stderr, "md: ");
	for (i = 0; i < 4; i++)
		fprintf(stderr, "%02x", md_value[i]);
	fprintf(stderr, "\n");

	fprintf(stderr, "ticket_md: ");
	for (i = 0; i < 4; i++)
		fprintf(stderr, "%02x", ticket_md[i]);
	fprintf(stderr, "\n");
#endif

	return memcmp(md_value, ticket_md, 4) == 0;
}

void get_data_id(chordID *id, const uchar *buf, int n)
{
	pack_hash(EVP_sha1(), id->x, buf, n, "");
}

void get_address_id(chordID *id, in6_addr *addr, ushort port)
{
	pack_hash(EVP_sha1(), id->x, 0, 0, "6s", addr, htons(port));
}

int verify_address_id(chordID *id, in6_addr *addr, ushort port)
{
	chordID correct_id;
	get_address_id(&correct_id, addr, port);
	return equals(&correct_id, id);
}

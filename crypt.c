#include <openssl/blowfish.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include "chord.h"

#undef C_DEBUG_ON

#ifdef C_DEBUG_ON
#define C_DEBUG(x) x
#else
#define C_DEBUG(x)
#endif

int vpack_hash(const EVP_MD *type, uchar *out, uchar *buf, int buf_len,
			   char *fmt, va_list args)
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
		printf("buf: ");
		int i;
		for (i = 0; i < buf_len; i++)
			printf("%02x", buf[i]);
		printf("\n");
#endif
	}

	for (; *fmt != '\0'; fmt++) {
		switch (*fmt) {
		case 'c':	 /* char */
			c = va_arg(args, int);
			EVP_DigestUpdate(&ctx, &c, 1);

			C_DEBUG(printf("char: %02x\n", c));
			break;
		case 's':	 /* short */
			s = va_arg(args, int);
			EVP_DigestUpdate(&ctx, &s, sizeof(ushort));

			C_DEBUG(printf("short: %hu\n", s));
			break;
		case 'l':	 /* long */
			l = va_arg(args, ulong);
			EVP_DigestUpdate(&ctx, &l, sizeof(ulong));

			C_DEBUG(printf("long: %lu\n", l));
			break;
		case 'x':	 /* id */
			id = va_arg(args, chordID *);
			EVP_DigestUpdate(&ctx, id->x, CHORD_ID_LEN);

			C_DEBUG(printf("chordID: "));
			C_DEBUG(print_id(stdout, id));
			C_DEBUG(printf("\n"));
			break;
		case '6':
			v6addr = va_arg(args, in6_addr *);
			EVP_DigestUpdate(&ctx, v6addr->s6_addr, 16);

			C_DEBUG(printf("addr: %s\n", v6addr_to_str(v6addr)));
			break;
		default:	 /* illegal type character */
			fprintf(stderr, "bad ticket type %c", *fmt);
			return 0;
		}
	}

	EVP_DigestFinal_ex(&ctx, out, NULL);
	EVP_MD_CTX_cleanup(&ctx);
}

int pack_hash(const EVP_MD *type, uchar *out, uchar *buf, int buf_len,
			  char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vpack_hash(type, out, buf, buf_len, fmt, args);
	va_end(args);
	return ret;
}

int pack_ticket(BF_KEY *key, uchar *out, char *fmt, ...)
{
	int i;
	va_list args;
	uchar md_value[EVP_MAX_MD_SIZE];
	uint32_t epoch_time = (uint32_t)time(NULL);

#ifdef C_DEBUG_ON
	printf("\ngenerating:\n");

	printf("time: ");
	for (i = 0; i < 4; i++)
		printf("%02x", ((uchar *)&epoch_time)[i]);
	printf(" %u\n", epoch_time);
#endif

	// hash the unix epoch time together with the caller's arguments
	va_start(args, fmt);
	vpack_hash(EVP_sha1(), md_value, (uchar *)&epoch_time, sizeof(epoch_time),
			   fmt, args);
	va_end(args);

	// pack the 32-bit epoch time and 32-bit hash into a buffer
	uchar ticket_value[TICKET_LEN];
	if (pack(ticket_value, "lcccc", epoch_time, md_value[0], md_value[1],
			 md_value[2], md_value[3]) < 0) {
		fprintf(stderr, "error packing ticket packet\n");
		return 0;
	}

	// and encrypt it using our secret function, which happens to be a single-
	// block blowfish cipher (note that the EVP_CIPHER_CTX* functions still
	// generate extraneous padding, even if we turn it off, so we'll call the
	// blowfish encryption function manually)
	BF_ecb_encrypt(ticket_value, out, key, BF_ENCRYPT);

#ifdef C_DEBUG_ON
	printf("ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		printf("%02x", ticket_value[i]);
	printf("\n");

	printf("encrypted ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		printf("%02x", out[i]);
	printf("\n");
#endif

	return 1;
}

int verify_ticket(BF_KEY *key, uchar *ticket_enc, char *fmt, ...)
{
	va_list args;
	uchar ticket[TICKET_LEN];
	uchar md_value[EVP_MAX_MD_SIZE];
	uchar ticket_md[4];
	uint32_t ticket_time;

	// decrypt the ticket
	BF_ecb_encrypt(ticket_enc, ticket, key, BF_DECRYPT);

#ifdef C_DEBUG_ON
	printf("\nverifying:\n");

	printf("encrypted ticket: ");
	int i;
	for (i = 0; i < TICKET_LEN; i++)
		printf("%02x", ticket_enc[i]);
	printf("\n");

	printf("ticket: ");
	for (i = 0; i < TICKET_LEN; i++)
		printf("%02x", ticket[i]);
	printf("\n");
#endif

	unpack(ticket, "lcccc", &ticket_time, &ticket_md[0], &ticket_md[1],
		   &ticket_md[2], &ticket_md[3]);

	if (ticket_time < time(NULL)-32)
		return 0;

	// hash together the time provided in the ticket with the data given in the
	// arguments (that were presumably in the packet received) to verify that
	// the remote host didn't modify our ticket
	va_start(args, fmt);
	vpack_hash(EVP_sha1(), md_value, (uchar *)&ticket_time, sizeof(ticket_time),
			   fmt, args);
	va_end(args);

#ifdef C_DEBUG_ON
	printf("time: ");
	for (i = 0; i < 4; i++)
		printf("%02x", ((uchar *)&ticket_time)[i]);
	printf(" %u\n", ticket_time);

	printf("md: ");
	for (i = 0; i < 4; i++)
		printf("%02x", md_value[i]);
	printf("\n");

	printf("ticket_md: ");
	for (i = 0; i < 4; i++)
		printf("%02x", ticket_md[i]);
	printf("\n");
#endif

	return memcmp(md_value, ticket_md, 4) == 0;
}

#ifdef HASH_PORT_WITH_ADDRESS
void get_address_id(chordID *id, in6_addr *addr, ushort port)
{
	pack_hash(EVP_sha1(), id->x, 0, 0, "6s", addr, port);
}

int verify_address_id(chordID *id, in6_addr *addr, ushort port)
{
	chordID correct_id;
	get_address_id(&correct_id, addr, port);
	return equals(&correct_id, id);
}
#else
void get_address_id(chordID *id, in6_addr *addr)
{
	pack_hash(EVP_sha1(), id->x, 0, 0, "6", addr);
}

int verify_address_id(chordID *id, in6_addr *addr)
{
	chordID correct_id;
	get_address_id(&correct_id, addr);
	return equals(&correct_id, id);
}
#endif

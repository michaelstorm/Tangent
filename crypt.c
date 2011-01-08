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

int pack_hash(uchar *out, uchar *buf, int buf_len, char *fmt, va_list args)
{
	EVP_MD_CTX ctx;
	char c;
	ushort s;
	ulong l;
	chordID *id;

	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_md5(), NULL);

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
			EVP_DigestUpdate(&ctx, id->x, ID_LEN);

			C_DEBUG(printf("chordID: "));
			C_DEBUG(print_id(stdout, id));
			C_DEBUG(printf("\n"));
			break;
		default:	 /* illegal type character */
			fprintf(stderr, "bad challenge type %c", *fmt);
			return 0;
		}
	}

	EVP_DigestFinal_ex(&ctx, out, NULL);
	EVP_MD_CTX_cleanup(&ctx);
}

int pack_challenge(BF_KEY *key, uchar *out, char *fmt, ...)
{
	va_list args;
	uchar md_value[16];
	uint32_t epoch_time = (uint32_t)time(NULL);

#ifdef C_DEBUG_ON
	printf("\ngenerating:\n");

	printf("time: ");
	int i;
	for (i = 0; i < 4; i++)
		printf("%02x", ((uchar *)&epoch_time)[i]);
	printf(" %u\n", epoch_time);
#endif

	va_start(args, fmt);
	pack_hash(md_value, (uchar *)&epoch_time, sizeof(epoch_time), fmt, args);
	va_end(args);

	uchar challenge_value[CHALLENGE_LEN];
	if (pack(challenge_value, "lcccc", epoch_time, md_value[0], md_value[1],
			 md_value[2], md_value[3]) < 0) {
		fprintf(stderr, "error packing challenge packet\n");
		return 0;
	}

	/*
	 * The EVP_CIPHER_CTX* functions still generate padding, even if we turn it
	 * off, so we'll do our single-block encryption manually.
	 */
	BF_ecb_encrypt(challenge_value, out, key, BF_ENCRYPT);

#ifdef C_DEBUG_ON
	printf("challenge: ");
	for (i = 0; i < CHALLENGE_LEN; i++)
		printf("%02x", challenge_value[i]);
	printf("\n");

	printf("encrypted challenge: ");
	for (i = 0; i < CHALLENGE_LEN; i++)
		printf("%02x", out[i]);
	printf("\n");
#endif

	return 1;
}

int verify_challenge(BF_KEY *key, uchar *challenge_enc, char *fmt, ...)
{
	va_list args;
	uchar challenge[CHALLENGE_LEN];
	uchar md_value[16];
	uchar challenge_md[4];
	uint32_t challenge_time;

	BF_ecb_encrypt(challenge_enc, challenge, key, BF_DECRYPT);

#ifdef C_DEBUG_ON
	printf("\nverifying:\n");

	printf("encrypted challenge: ");
	int i;
	for (i = 0; i < CHALLENGE_LEN; i++)
		printf("%02x", challenge_enc[i]);
	printf("\n");

	printf("challenge: ");
	for (i = 0; i < CHALLENGE_LEN; i++)
		printf("%02x", challenge[i]);
	printf("\n");
#endif

	unpack(challenge, "lcccc", &challenge_time, &challenge_md[0],
		   &challenge_md[1], &challenge_md[2], &challenge_md[3]);

	if (challenge_time < time(NULL)-64)
		return 0;

	va_start(args, fmt);
	pack_hash(md_value, (uchar *)&challenge_time, sizeof(challenge_time), fmt,
			  args);
	va_end(args);

#ifdef C_DEBUG_ON
	printf("time: ");
	for (i = 0; i < 4; i++)
		printf("%02x", ((uchar *)&challenge_time)[i]);
	printf(" %u\n", challenge_time);

	printf("md: ");
	for (i = 0; i < 4; i++)
		printf("%02x", md_value[i]);
	printf("\n");

	printf("challenge_md: ");
	for (i = 0; i < 4; i++)
		printf("%02x", challenge_md[i]);
	printf("\n");
#endif

	return memcmp(md_value, challenge_md, 4) == 0;
}

int encrypt(const uchar *clear, int len, uchar *encrypted, uchar *key,
			uchar *iv)
{
	int encrypted_len;
	int encrypted_final_len;

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_CIPHER_CTX_set_padding(&ctx, EVP_CIPH_NO_PADDING);
	EVP_EncryptInit_ex(&ctx, EVP_bf_cbc(), NULL, key, iv);

	if (!EVP_EncryptUpdate(&ctx, encrypted, &encrypted_len, clear, len))
		return 0;

	if (!EVP_EncryptFinal_ex(&ctx, encrypted + encrypted_len, &encrypted_final_len))
		return 0;

	EVP_CIPHER_CTX_cleanup(&ctx);
	return encrypted_len + encrypted_final_len;
}

int decrypt(const uchar *encrypted, int len, uchar *clear, uchar *key,
			uchar *iv)
{
	int clear_len;
	int clear_final_len;

	EVP_CIPHER_CTX ctx;
	EVP_CIPHER_CTX_init(&ctx);
	EVP_CIPHER_CTX_set_padding(&ctx, EVP_CIPH_NO_PADDING);
	EVP_DecryptInit_ex(&ctx, EVP_bf_cbc(), NULL, key, iv);

	if (!EVP_DecryptUpdate(&ctx, clear, &clear_len, encrypted, len))
		return 0;

	if (!EVP_DecryptFinal_ex(&ctx, clear + clear_len, &clear_final_len))
		return 0;

	return clear_len + clear_final_len;
}

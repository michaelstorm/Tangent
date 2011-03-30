#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "chord.h"
#include "gen_utils.h"

/* f_rand: return a random double between 0.0 and 1.0 */
double f_rand(void)
{
	int64_t l = (int64_t)(random() & ((1 << 26) - 1));
	int64_t r = (int64_t)(random() & ((1 << 27) - 1));
	return ((l << 27) + r) / (double)(1LL << 53);
}

/**********************************************************************/

/* funif_rand: Return a random number between a and b */
double funif_rand(double a, double b)
{
	return a + (b - a) * f_rand();
}

/**********************************************************************/

/* n_rand: return a random integer in [0, n),
   borrowed from Java Random class */
int n_rand(int n)
{
	int bits, val;

	assert(n > 0);	 /* n must be positive */

	/* Special case: power of 2 */
	if ((n & -n) == n)
		return random() & (n - 1);

	do {
		bits = random();
		val = bits % n;
	} while (bits - val + (n - 1) < 0);
	return val;
}

/**********************************************************************/

/* unif_rand: return a random integer number in the interval [a, b) */
int unif_rand(int a, int b)
{
	return a + n_rand(b - a);
}

/**********************************************************************/

/* getusec: return wall time in usec */
uint64_t wall_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/**********************************************************************/

void update_rtt(long *rtt_avg, long *rtt_dev, long new_rtt)
{
	long err;

	if (*rtt_avg == 0) {
		*rtt_avg = new_rtt;
		*rtt_dev = 0;
		return;
	}

	/* use TCP's rtt estimation algorithm */
	err = new_rtt - *rtt_avg;
	*rtt_avg += err >> 3;
	if (err < 0) err = -err;
		*rtt_dev += (err - *rtt_dev) >> 2;
}


/**********************************************************************/

/* randID: return a random ID */
chordID rand_ID()
{
	chordID id;

	int i;
	for (i = 0; i < CHORD_ID_LEN; i++)
		id.x[i] = (uchar)(random() & 0xff);
	return id;
}

/**********************************************************************/

/* successorID: id + (1 << n) */
chordID successor(chordID id, int n)
{
	uchar old;
	int start, i;

	assert(n >= 0 && n < CHORD_ID_BITS);
	/* Note id.x[0] is most significant bit */
	start = CHORD_ID_LEN-1 - n/8;
	old = id.x[start];
	id.x[start] += 1 << (n%8);
	if (id.x[start] < old) {
		for (i = start-1; i >= 0; i--) {
			id.x[i]++;
			if (id.x[i])
				break;
		}
	}
	return id;
}

/**********************************************************************/

/* predecessorID: id - (1 << n) */
chordID predecessor(chordID id, int n)
{
	uchar old;
	int start, i;

	assert(n >= 0 && n < CHORD_ID_BITS);
	start = CHORD_ID_LEN-1 - n/8;
	old = id.x[start];
	id.x[start] -= 1 << (n%8);
	if (id.x[start] > old) {
		for (i = start-1; i >= 0; i--) {
			if (id.x[i]) {
				id.x[i]--;
				break;
			}
			else
				id.x[i]--;
		}
	}
	return id;
}

/**********************************************************************/

/* add: res = a + b (mod 2^n) */
void add(chordID *a, chordID *b, chordID *res)
{
	int i, carry = 0;
	for (i = CHORD_ID_LEN - 1; i >= 0; i--) {
		res->x[i] = (a->x[i] + b->x[i] + carry) & 0xff;
		carry = (a->x[i] + b->x[i] + carry) >> 8;
	}
}

/**********************************************************************/

/* subtract: res = a - b (mod 2^n) */
void subtract(chordID *a, chordID *b, chordID *res)
{
	int i, borrow = 0;
	for (i = CHORD_ID_LEN - 1; i >= 0; i--) {
		if (a->x[i] - borrow < b->x[i]) {
			res->x[i] = 256 + a->x[i] - borrow - b->x[i];
			borrow = 1;
		} else {
			res->x[i] = a->x[i] - borrow - b->x[i];
			borrow = 0;
		}
	}
}

/**********************************************************************/

chordID random_from(chordID *a)
{
	chordID b;
	int	m = random()%10 + 1;

	int i;
	for (i = 0; i < CHORD_ID_LEN; i++)
		b.x[i] = a->x[i]*m/11;
	return b;
}

/**********************************************************************/

void random_between(chordID *a, chordID *b, chordID *res)
{
	subtract(b, a, res);
	chordID r = random_from(res);
	add(a, &r, res);
}

/**********************************************************************/

static int msb_tab[256] = {
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

/* msb: most significant bit */
int msb(chordID *x)
{
	int i;
	for (i = 0; i < CHORD_ID_LEN; i++)
	if (x->x[i])
		return 8 * i + msb_tab[x->x[i]];
	return 0;
}

/**********************************************************************/

/* equals: a == b? */
int equals(chordID *a, chordID *b)
{
	return memcmp(a->x, b->x, sizeof(chordID)) == 0;
}

/* equals: a == b? */
int equals_id_str(chordID *a, char *b)
{
	chordID idb = atoid(b);
	return memcmp(a->x, idb.x, sizeof(chordID)) == 0;
}

/**********************************************************************/

int is_zero(chordID *x)
{
	static chordID zero;
	return memcmp(x->x, zero.x, sizeof(chordID)) == 0;
}

/**********************************************************************/

/* less: a<b? */
static int is_less(chordID *a, chordID *b)
{
	return memcmp(a->x, b->x, sizeof(chordID)) < 0;
}

/* between: is x in (a,b) on circle? */
int is_between(chordID *x, chordID *a, chordID *b)
{
	if (equals(a, b))
		return !equals(x, a);	 /* x is only node not in (x,x) */
	else if (is_less(a, b))
		return is_less(a, x) && is_less(x, b);
	else
		return is_less(a, x) || is_less(x, b);
}

/***********************************************/
int copy_id( chordID *a, chordID *b)
{
	assert(a);
	assert(b);

	int i;
	for (i = 0; i < sizeof(chordID); i++)
		a->x[i] = b->x[i];
	return 1;
}

/**********************************************************************/

void print_id(FILE *f, chordID *id)
{
	int i;
	for (i = 0; i < CHORD_ID_LEN; i++)
		fprintf(f, "%02x", id->x[i]);
}

/**********************************************************************/

static unsigned char todigit(char ch)
{
	if (isdigit((int) ch))
		return (ch - '0');
	else
		return (10 + ch - 'a');
}

chordID atoid(const char *str)
{
	chordID id;
	int i;

	assert(strlen(str) == 2*CHORD_ID_LEN);
	for (i = 0; i < CHORD_ID_LEN; i++)
		 id.x[i] = (todigit(str[2*i]) << 4) | todigit(str[2*i+1]);
	return id;
}

/**********************************************************************/

enum {
	MULTIPLIER = 31			 /* for hash() */
};

/* hash: compute hash value for ID */
unsigned hash(chordID *id, unsigned n)
{
	unsigned h = 0;
	int i;

	for (i = 0; i < CHORD_ID_LEN; i++)
		h = MULTIPLIER * h + id->x[ i ];

	return h % n;
}

int match_key(chordID *key_array, int num_keys, chordID *key)
{
	int i;
	for (i = 0; i < num_keys; i++) {
		if (memcmp((char *)&key->x[0], (char *)&key_array[i].x[0], CHORD_ID_LEN) == 0)
			return 1;
	}
	return 0;
}

/***********************************************************************/

void print_chordID(chordID *id)
{
	if (id) {
		int i;
#ifdef CHORD_PRINT_LONG_IDS
		for (i = 0; i < CHORD_ID_LEN; i++)
			fprintf(stderr, "%02x", id->x[i]);
#else
		for (i = 0; i < 4; i++)
			fprintf(stderr, "%02x", id->x[i]);
#endif
	}
	else
		fprintf(stderr, "<null>");
}

/***********************************************************************/

void print_two_chordIDs(char *preffix, chordID *id1,
						char *middle, chordID *id2,
						char *suffix)
{
	assert(preffix && id1 && middle && id2 && suffix);
	fprintf(stderr, "%s", preffix);
	print_chordID(id1);
	fprintf(stderr, "%s", middle);
	print_chordID(id2);
	fprintf(stderr, "%s", suffix);
}

/***********************************************************************/

void print_three_chordIDs(char *preffix, chordID *id1,
						  char *middle1, chordID *id2,
						  char *middle2, chordID *id3,
						  char *suffix)
{
	assert(preffix && id1 && middle1 && id2 && middle2 && id3 && suffix);
	fprintf(stderr, "%s", preffix);
	print_chordID(id1);
	fprintf(stderr, "%s", middle1);
	print_chordID(id2);
	fprintf(stderr, "%s", middle2);
	print_chordID(id3);
	fprintf(stderr, "%s", suffix);
}


/***********************************************************************/

void print_node(Node *node, char *prefix, char *suffix)
{
	fprintf(stderr, "%s", prefix);
	print_chordID(&node->id);

	char *addr_str = v6addr_to_str(&node->addr);
	fprintf(stderr, ", %s, %d%s", addr_str, node->port, suffix);
}

void print_finger(Finger *f, char *prefix, char *suffix)
{
	fprintf(stderr, "%sFinger:", prefix);
	print_node(&f->node, "<", ">");
	fprintf(stderr, " (status = %s, npings = %d, rtt = %ld/%ld) %s",
		   f->status ? "ACTIVE" : "PASSIVE", f->npings, f->rtt_avg, f->rtt_dev,
		   suffix);
}


void print_finger_list(Finger *fhead, char *prefix, char *suffix)
{
	int i;
	Finger *f;

	fprintf(stderr, "%s", prefix);
	for (f = fhead, i = 0; f; f = f->next, i++) {
		fprintf(stderr, "	[%d] ", i);
		print_finger(f, "", "\n");
	}
	fprintf(stderr, "%s", suffix);
}

void print_server(Server *s, char *prefix, char *suffix)
{
	fprintf(stderr, "---------------%s---------------\n", prefix);
	print_node(&s->node, "[", "]\n");
	fprintf(stderr, "(%d passive)\n", s->num_passive_fingers);
	print_finger_list(s->head_flist, "	Finger list:\n", "\n");
	fprintf(stderr, "---------------%s---------------\n", suffix);
}


void print_process(Server *srv, char *process_type, chordID *id, in6_addr *addr,
				   ushort port)
{
#define TYPE_LEN 16
	int i = TYPE_LEN - strlen(process_type);

	fprintf(stderr, "[%s]", process_type);
	if (i > 0) for (; i; i--) fprintf(stderr, " ");

	fprintf(stderr, " (");
	if (id)
		print_chordID(id);
	else
		fprintf(stderr, "null");
	fprintf(stderr, ") ");
	print_node(&srv->node, " <", ">");
	if (addr == NULL)
		fprintf(stderr, " <----- <,>");
	else
		fprintf(stderr, " <----- <%s, %d>", v6addr_to_str(addr), port);
	print_current_time(" Time:", "\n");
}

void print_send(Server *srv, char *send_type, chordID *id, in6_addr *addr,
				ushort port)
{
	int i = TYPE_LEN - strlen(send_type);

	fprintf(stderr, "[%s]", send_type);
	if (i > 0) for (; i; i--) fprintf(stderr, " ");

	fprintf(stderr, " (");
	if (id)
		print_chordID(id);
	else
		fprintf(stderr, "null");
	fprintf(stderr, ") ");
	print_node(&srv->node, " <", ">");
	if (addr == NULL)
		fprintf(stderr, " -----> <,>");
	else
		fprintf(stderr, " -----> <%s, %d>", v6addr_to_str(addr), port);
	print_current_time(" Time:", "\n");
}

void print_fun(Server *srv, char *fun_name, chordID *id)
{
	fprintf(stderr, "%s: ", fun_name);
	print_chordID(&srv->node.id);
	fprintf(stderr, " > ");
	print_chordID(id);
	print_current_time(" @ ", "\n");
}

ulong get_current_time()
{
	return (ulong)wall_time();
}

void print_current_time(char *prefix, char *suffix)
{
#ifdef CHORD_PRINT_LONG_TIME
	fprintf(stderr, "%s%lld%s", prefix, wall_time(), suffix);
#else
	fprintf(stderr, "%s%lld%s", prefix, (wall_time() << 32) >> 32, suffix);
#endif
}

int v6_addr_equals(const in6_addr *addr1, const in6_addr *addr2)
{
	return memcmp(addr1->s6_addr, addr2->s6_addr, 16) == 0;
}

void v6_addr_copy(in6_addr *dest, const in6_addr *src)
{
	memcpy(dest->s6_addr, src->s6_addr, 16);
}

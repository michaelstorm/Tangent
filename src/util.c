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
#include "chord/chord.h"
#include "chord/gen_utils.h"
#include "chord/finger.h"
#include "chord/util.h"

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

/* successorID: id + (1 << n) */
chordID chord_id_successor(chordID id, int n)
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
chordID chord_id_predecessor(chordID id, int n)
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
void id_add(chordID *a, chordID *b, chordID *res)
{
	int i, carry = 0;
	for (i = CHORD_ID_LEN - 1; i >= 0; i--) {
		res->x[i] = (a->x[i] + b->x[i] + carry) & 0xff;
		carry = (a->x[i] + b->x[i] + carry) >> 8;
	}
}

/**********************************************************************/

/* subtract: res = a - b (mod 2^n) */
void id_subtract(chordID *a, chordID *b, chordID *res)
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

void id_random_between(chordID *a, chordID *b, chordID *res)
{
	id_subtract(b, a, res);
	chordID r = random_from(res);
	id_add(a, &r, res);
}

/**********************************************************************/

/* equals: a == b? */
int id_equals(chordID *a, chordID *b)
{
	return memcmp(a->x, b->x, sizeof(chordID)) == 0;
}

/**********************************************************************/

/* less: a<b? */
static int is_less(chordID *a, chordID *b)
{
	return memcmp(a->x, b->x, sizeof(chordID)) < 0;
}

/* between: is x in (a,b) on circle? */
int id_is_between(chordID *x, chordID *a, chordID *b)
{
	if (id_equals(a, b))
		return !id_equals(x, a);	 /* x is only node not in (x,x) */
	else if (is_less(a, b))
		return is_less(a, x) && is_less(x, b);
	else
		return is_less(a, x) || is_less(x, b);
}

/**********************************************************************/

const char *id_to_str(chordID *id)
{
	static char id_str[CHORD_ID_LEN*2+1];
	if (id) {
		int i;
#ifdef CHORD_PRINT_LONG_IDS
		for (i = 0; i < CHORD_ID_LEN; i++)
#else
		for (i = 0; i < 4; i++)
#endif
			sprintf(id_str+i*2, "%02x", id->x[i]);
		id_str[CHORD_ID_LEN*2] = '\0';
	}
	else
		id_str[0] = '\0';
	return id_str;
}

/***********************************************************************/

void print_chordID(FILE* out, chordID *id)
{
	if (id) {
		int i;
#ifdef CHORD_PRINT_LONG_IDS
		for (i = 0; i < CHORD_ID_LEN; i++)
#else
		for (i = 0; i < 4; i++)
#endif
			fprintf(out, "%02x", id->x[i]);
	}
	else
		fprintf(out, "<null>");
}

/***********************************************************************/

void print_two_chordIDs(FILE *out, char *prefix, chordID *id1,
						char *middle, chordID *id2,
						char *suffix)
{
	assert(prefix && id1 && middle && id2 && suffix);
	fprintf(out, "%s", prefix);
	print_chordID(out, id1);
	fprintf(out, "%s", middle);
	print_chordID(out, id2);
	fprintf(out, "%s", suffix);
}

/***********************************************************************/

void print_node(FILE *out, Node *node)
{
	fprintf(out, "[");
	print_chordID(out, &node->id);

	char *addr_str = v6addr_to_str(&node->addr);
	fprintf(out, ", %s, %d]", addr_str, node->port);
}

void print_finger(FILE *out, Finger *f, char *prefix, char *suffix)
{
	fprintf(out, "%sFinger:", prefix);
	print_node(out, &f->node);
	fprintf(out, " (status = %s, npings = %d, rtt = %ld/%ld) %s",
		   f->status ? "ACTIVE" : "PASSIVE", f->npings, f->rtt_avg, f->rtt_dev,
		   suffix);
}

void print_finger_list(FILE *out, Finger *fhead)
{
	int i;
	Finger *f;

	for (f = fhead, i = 0; f; f = f->next, i++) {
		fprintf(out, "	[%d] ", i);
		print_finger(out, f, "", "");
	}
}

void print_server(FILE *out, ChordServer *s)
{
	print_node(out, &s->node);
	fprintf(out, "\n(%d passive)\n", s->num_passive_fingers);
	fprintf(out, "Finger list:\n");
	print_finger_list(out, s->head_flist);
}


void print_process(FILE *out, ChordServer *srv, char *process_type, chordID *id, in6_addr *addr,
				   ushort port)
{
#define TYPE_LEN 16
	//int i = TYPE_LEN - strlen(process_type);

	//fprintf(out, "[%s]", process_type);
	//if (i > 0) for (; i; i--) fprintf(out, " ");

	fprintf(out, "(");
	if (id)
		print_chordID(out, id);
	else
		fprintf(out, "null");
	fprintf(out, ") ");
	print_node(out, &srv->node);
	if (addr == NULL)
		fprintf(out, " <----- <,>");
	else
		fprintf(out, " <----- <%s, %d>", v6addr_to_str(addr), port);
	print_current_time(out, " Time:", "");
}

void print_send(FILE *out, ChordServer *srv, char *send_type, chordID *id, in6_addr *addr, ushort port)
{
	//int i = TYPE_LEN - strlen(send_type);

	//fprintf(out, "[%s]", send_type);
	//if (i > 0) for (; i; i--) fprintf(out, " ");

	fprintf(out, "(");
	print_chordID(out, id);
	fprintf(out, ") ");
	
	print_node(out, &srv->node);
	if (addr == NULL)
		fprintf(out, " -----> <,>");
	else
		fprintf(out, " -----> <%s, %d>", v6addr_to_str(addr), port);
	print_current_time(out, " Time:", "");
}

void print_fun(FILE *out, ChordServer *srv, char *fun_name, chordID *id)
{
	fprintf(out, "%s: ", fun_name);
	print_chordID(out, &srv->node.id);
	fprintf(out, " > ");
	print_chordID(out, id);
	print_current_time(out, " @ ", "");
}

ulong get_current_time()
{
	return (ulong)wall_time();
}

void print_current_time(FILE *out, char *prefix, char *suffix)
{
#ifdef CHORD_PRINT_LONG_TIME
	fprintf(out, "%s%"PRIu64"%s", prefix, wall_time(), suffix);
#else
	fprintf(out, "%s%"PRIu64"%s", prefix, (wall_time() << 32) >> 32, suffix);
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

void v6_addr_set(in6_addr *dest, const uchar *src)
{
	memcpy(dest->s6_addr, src, 16);
}

char *buf_to_str(const uchar *buf, int len)
{
	static char buf_str[1024];
	memcpy(buf_str, buf, len);
	buf_str[len] = '\0';
	return buf_str;
}

char *buf_to_hex(const uchar *buf, int len)
{
	static char buf_hex[1024];
	int i;
	for (i = 0; i < len; i++)
		sprintf(buf_hex+i*2, "%02x", buf[i]);
	buf_hex[len*2] = '\0';
	return buf_hex; 
}

void to_v6addr(ulong v4addr, in6_addr *v6addr)
{
	memset(&v6addr->s6_addr[0], 0, 10);
	memset(&v6addr->s6_addr[10], 0xFF, 2);
	memcpy(&v6addr->s6_addr[12], &v4addr, 4);
}

ulong to_v4addr(const in6_addr *v6addr)
{
	return *(ulong *)&v6addr->s6_addr[12];
}

char *v6addr_to_str(const in6_addr *v6addr)
{
	static char addr_str[INET6_ADDRSTRLEN];
	if (!V4_MAPPED(v6addr))
		inet_ntop(AF_INET6, v6addr, addr_str, INET6_ADDRSTRLEN);
	else {
		ulong v4addr = to_v4addr(v6addr);
		inet_ntop(AF_INET, &v4addr, addr_str, INET6_ADDRSTRLEN);
	}
	return addr_str;
}

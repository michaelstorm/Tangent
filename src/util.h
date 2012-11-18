#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

double f_rand();
double funif_rand(double a, double b);
int n_rand(int n);
int unif_rand(int a, int b);
uint64_t wall_time();
ulong get_current_time();
void update_rtt(long *rtt_avg, long *rtt_std, long new_rtt);
chordID rand_ID();
chordID successor(chordID id, int n);
chordID predecessor(chordID id, int n);
void add(chordID *a, chordID *b, chordID *res);
void subtract(chordID *a, chordID *b, chordID *res);
void random_between(chordID *a, chordID *b, chordID *res);
int msb(chordID *x);
int equals(chordID *a, chordID *b);
int equals_id_str(chordID *a, char *b);
int is_zero(chordID *x);
int is_between(chordID *x, chordID *a, chordID *b);
int copy_id( chordID *a, chordID *b);
chordID atoid(const char *str);
unsigned hash(chordID *id, unsigned n);
const char *chordID_to_str(chordID *id);
void print_chordID(FILE *out, chordID *id);
void print_two_chordIDs(FILE *out, char *preffix, chordID *id1,
						char *middle, chordID *id2,
						char *suffix);
void print_three_chordIDs(FILE *out, char *preffix, chordID *id1,
						  char *middle1, chordID *id2,
						  char *middle2, chordID *id3,
						  char *suffix);
void print_node(FILE *out, Node *node);
void print_finger(FILE *out, Finger *f, char *prefix, char *suffix);
void print_finger_list(FILE *out, Finger *fhead);
void print_server(FILE *out, Server *s);
void print_send(FILE *out, Server *srv, char *send_type, chordID *id, in6_addr *addr,
				ushort port);
void print_process(FILE *out, Server *srv, char *process_type, chordID *id, in6_addr *addr,
				   ushort port);
void print_fun(FILE *out, Server *srv, char *fun_name, chordID *id);
void print_current_time(FILE *out, char *prefix, char *suffix);
int match_key(chordID *key_array, int num_keys, chordID *key);
int v6_addr_equals(const in6_addr *addr1, const in6_addr *addr2);
void v6_addr_copy(in6_addr *dest, const in6_addr *src);
void v6_addr_set(in6_addr *dest, const uchar *src);
char *buf_to_str(const uchar *buf, int len);
char *buf_to_hex(const uchar *buf, int len);
void get_data_id(chordID *id, const uchar *buf, int n);
void get_address_id(chordID *id, in6_addr *addr, ushort port);
int verify_address_id(chordID *id, in6_addr *addr, ushort port);
void to_v6addr(ulong v4addr, in6_addr *v6addr);
ulong to_v4addr(const in6_addr *v6addr);
char *v6addr_to_str(const in6_addr *v6addr);

#ifdef __cplusplus
}
#endif

#endif
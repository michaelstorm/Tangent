#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "incl.h"

Server server[MAX_NUM_SERVERS];
int    server_status[MAX_NUM_SERVERS];

void init_global_data()
{
  int i;

  for (i = 0; i < MAX_NUM_SERVERS; i++) 
    server_status[i] = SRV_ABSENT;
}


int get_server_status(int idx)
{
  assert(idx >= 0 && idx < MAX_NUM_SERVERS);

  return server_status[idx];
}

void set_server_status(int idx, int status)
{
  assert(idx >= 0 && idx < MAX_NUM_SERVERS);

  server_status[idx] = status;
}

Server *get_server(int idx)
{
  assert(idx >= 0 && idx < MAX_NUM_SERVERS);

  return &server[idx];
}

Server *get_server_by_id(chordID *id)
{
  int i = 0;

  for (i = 0; i < MAX_NUM_SERVERS; i++)
    if ((server_status[i] != SRV_ABSENT) && equals(&server[i].node.id, id))
      return &server[i];

  return NULL;
}
      

Server *get_random_server(int no_idx, int status)
{
  int i, idx = 0;

  for (i = 0; i < MAX_NUM_SERVERS; i++)
    if (server_status[i] == status && i != no_idx) 
      idx++;

  if (!idx)
    return NULL;

  idx = random() % idx;

  for (i = 0; i < MAX_NUM_SERVERS; i++) {
    if ((server_status[i] == status) && (idx == 0) && (i != no_idx)) 
      return &server[i];
    idx--;
  }

  return NULL;
}


int add_server(chordID *id, int status)
{
  static int crt_entry = 0;
  int last_entry = crt_entry;

  assert (status != SRV_ABSENT);

  /* look for an empty position */
  for (; crt_entry < MAX_NUM_SERVERS; crt_entry++)
    if (server_status[crt_entry] == SRV_ABSENT) 
      break;

  if (crt_entry == MAX_NUM_SERVERS) {
    for (; crt_entry < last_entry; crt_entry++)
      if (server_status[crt_entry] == SRV_ABSENT) 
	break;
    if (crt_entry == last_entry)
      return -1;
  }
  
  /* add new server at position "crt_entry" */
  memset(&server[crt_entry], 0, sizeof(Server));
  server[crt_entry].node.id = *id;
  server[crt_entry].node.addr = crt_entry;
  server_status[crt_entry] = status;
  return crt_entry;
}

/*************************************************************************/

// initiate join request
void sim_join(Server *srv, chordID *id) 
{
  assert(get_server_status(srv->node.addr) == SRV_TO_JOIN);
  sim_stabilize(srv);
  set_server_status(srv->node.addr, SRV_PRESENT);
}

void sim_stabilize(Server *srv)
{
  stabilize(srv);
  server_status[srv->node.addr] = SRV_PRESENT;

  if (get_server_status(srv->node.addr) == SRV_ABSENT)
    return;
  genEvent(srv->node.addr, sim_stabilize, (void *)NULL, 
	   Clock + unifRand(0.5*SIM_STABILIZE_PERIOD, 
			    1.5*SIM_STABILIZE_PERIOD));
}

void sim_fail(Server *srv)
{
  server_status[srv->node.addr] = SRV_ABSENT;
}


/* invoked when a control or a data packet is received at
 * an intermediate node
 */
void sim_recv_message(Server *srv, uchar *sp)
{
  int n = *(int *)sp;
  
  dispatch(srv, n, sp + sizeof(int));
}


/* invoked when data packet reaches destination */
void sim_deliver_data(Server *srv, chordID *id, int n, uchar *data)
{
  printf("Packet ");
  print_chordID(id);
  printf(" delivered to node ");
  print_chordID(&srv->node.id);
  print_current_time(" @ ", "");
  printf("\n");
}


/* send a data packet with ID "id" from server "srv" */
void sim_send_data(Server *srv, chordID *id)
{
  static char dummy_buf[2];
  int len = 2;

  process_data(srv, CHORD_ROUTE, DEF_TTL, id, len, dummy_buf);
}

/* send_raw: send datagram to remote addr:port */
void sim_send_raw(Server *srv, in_addr_t addr, 
		  in_port_t port, int n, uchar *buf)
{
  char *sp;

  // XXX
  //static uint32_t cnt = 0, once = 0;
  //if (Clock > 974269 && Clock < 1074730) 
  //  cnt++;
  //if (Clock > 1074730 && once == 0) {
  //  once = 1;
  //  printf("number of messages = %u\n", cnt);
  //}
  // XXX
  if ((sp = malloc(sizeof(int) + n)) == NULL) {
    printf("send_raw: memory allocation error.\n");
    exit(-1);
  }

  *(int *)sp = n;
  memcpy(sp + sizeof(int), buf, n);

  genEvent(addr, sim_recv_message, (void *)sp, 
	   Clock + intExp(SIM_AVG_PKT_DELAY));
}


/**********************************************************************/

int sim_chord_is_local(Server *srv, chordID *x)
{
  chordID l, r;
  Finger *f;

  r = srv->node.id;
  f = pred_finger(srv);
  if (!f)
    return TRUE;
  else
    l = f->node.id;

  return equals(x, &r) || is_between(x, &l, &r);
}

/**********************************************************************/

double sim_get_time()
{
  return Clock;
}


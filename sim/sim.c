/*
 *
 * Copyright (C) 2001 Ion Stoica (istoica@cs.berkeley.edu)
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#define DECL_VAR
#include "incl.h"

/* this variables are just needed to compile ../utils.c ! */
Key KeyArray[MAX_KEY_NUM];
int NumKeys;

void exitSim(void);
void test_finger(void);

int main(int argc, char **argv) 
{
  Event *ev;

  //initRand(1);
  //test_finger();
  //exit(-1);
  
  if (argc != 3) {
    printf("usage: %s input.ev seed\n", argv[0]);
    exit (-1);
  }

  EventQueue.q = initEventQueue();
  EventQueue.size = MAX_NUM_ENTRIES;

  initRand(atoi(argv[2]));

  init_global_data();

  readInputFile(argv[1]);

  while (Clock < MAX_TIME) {
    ev = getEvent(&EventQueue, Clock);
    if (!ev) 
      Clock = (double)((int)(Clock/ENTRY_TUNIT) + 1.0)*ENTRY_TUNIT; 
    else {
      Clock = ev->time;
      {
        static double tsec = 0.;
        if (ev->time/1000000. > tsec) {
          tsec += 1.0;
	}
      }
      if (ev->fun == exitSim) {
	exitSim();
      } 
      if (get_server_status(ev->idx) != SRV_ABSENT) {
	ev->fun(get_server(ev->idx), ev->params);
      }
      free(ev);
    }
  }
  return 0;
}


void exitSim(void)
{
  int i;

  for (i = 0; i < MAX_NUM_SERVERS; i++)
    if (get_server_status(i) == SRV_PRESENT)
      print_server(get_server(i), "exitSim", "");
  printf("Exit: %f\n", Clock);
  exit(0);
}

int test_pick_random_id(int *present, int len, int status)
{
  int i, cnt = 0;

  for (i = 0; i < len; i++)
    if (present[i] == status)
      cnt++;
  if (cnt == 0)
    return -1;

  cnt = unifRand(1, cnt);

  for (i = 0; ; i++) {
    if (present[i] == status)
      cnt--;
    if (cnt == 0)
      return i;
  }
}


void test_finger()
{
#define TEST_NUM_IDS 100
  chordID id_list[TEST_NUM_IDS];
  int     present[TEST_NUM_IDS];
  int i, idx, k, flag;
  Server srv;
  Finger *f;
  int     fnew;

  memset(&srv, 0, sizeof(Server));
  srv.node.id = rand_ID();
  srv.node.addr = 100000;
  srv.node.port = 0;

  for (i = 0; i < TEST_NUM_IDS; i++) {
    present[i] = 0;
    flag = TRUE;
    while (flag) {
      id_list[i] = rand_ID();
      flag = FALSE;
      for (k = 0; k < i; k++) {
	if (equals(&id_list[i], &id_list[k]) || 
	    equals(&id_list[i], &srv.node.id)) {
	  flag = TRUE;
	  break;
	}
      }
    }
  }

  for (k = 0; TRUE; k++) {
    if (funifRand(0, 1.) <= 0.5) {
      idx = test_pick_random_id(present, TEST_NUM_IDS, FALSE);
      if (idx == -1)
	continue;
      //printf("k = %d, insert %d\n", k, idx);
      insert_finger(&srv, &id_list[idx], idx, 0, &fnew);
      present[idx] = TRUE;
    } else {
      idx = test_pick_random_id(present, TEST_NUM_IDS, TRUE);
      if (idx == -1)
	continue;
      //printf("k = %d, remove %d\n", k, idx);
      if ((f = get_finger(&srv, &id_list[idx])) == NULL) {
	printf("Finger %d not found!\n", idx);
	exit(-1);
      }
      remove_finger(&srv, f);
      present[idx] = FALSE;
    }
  }
}
      






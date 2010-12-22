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


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "incl.h"
#include "traffic_gen.h"

#define NUM_DUPLICATE_ROUTES 2

/* this variables are just needed to compile ../utils.c ! */
Key KeyArray[MAX_KEY_NUM];
int NumKeys;

static int NumNodes = 0;

// This program reads a script file and generates
// a list of events to be processed by ./sim
//
// usage: ./traffic_gen input_file seed 
//   
// - input_file can contain three commands
//
//  1) events num avg wjoin wleave wfail winsert wfind
//    
//    Description: this command generates join, leave, fail
//    insert document, and find document events 
//
//     num - represents the total number of events to be 
//           generated
//     avg - represents the average distance in ms between
//           two consecutive events; this distance is 
//           randomly distributed
//     wjoin, wfail, wlookup - represent weights
//           associated to each event type; an even of a certain
//           type is generated with a probability inverese 
//           proportional to its weight
//
//  2) wait time
//
//     Description: this commad generate an even that inserts a
//     pause in the simulation (usually this command is used to 
//     wait for network stabilization)
//
//  3) exit 
//
//     Description: generate an even to end simulation 
//
//  Example:
//
// > cat input_file
// events 1000 10000 100 0 0  
// wait 60000
// events 1000 1000 0 0 0 
// wait 60000
// events 10000 1000 10 10 10
// wait 60000
// exit
// >
//
// Description: 
//
// - 1st line creates a network of 1000 nodes; the command
//   will generate 1000 node join operations with an average frequency
//   of 1/10000ms = 1/10 sec
//
// - 2nd line inserts a pause of 1 min (60000ms); waiting for the
//   network to stabilize
//
// - 3rd line inserts 1000 documents in the network with a frequency
//   of one document per second
//
// - 4th line inserts a 1 sec pause
//
// - 5th line generates 10000 joins, leaves, finds, and inserts with a
//   frequency of one event per second
//
// - 6th line introduces a 1-sec pause; waiting for all outstanding
//   operation to finish
//
// - 7th line generates the end of the simulation
//

int main(int argc, char **argv) 
{
  int i;

  if (argc != 3) {
    printf("usage: %s input.sc seed\n", argv[0]);
    exit (-1);
  }

  initRand(atoi(argv[2]));

  for (i = 0; i < MAX_NUM_NODES; i++)
    ID_present[i] = FALSE;

  readInputFileGen(argv[1]);


  return 0;
}



void readInputFileGen(char *file)
{
  FILE *fp;
  char ch;

  if ((fp = fopen(file, "r")) == NULL) {
    printf("%s: file open error.\n", file);
    panic("");
  }

  while (!feof(fp)) {
    if (((ch = getc(fp)) == EOL) || (ch == 0xd))
      continue;
    if (ch == '#') {
      ignoreCommentLineGen(fp);
      continue;
    } else {
      if (feof(fp))
	break;
      ungetc(ch, fp);
    }
    readLineGen(fp);
  }
}


void readLineGen(FILE *fp)
{
  char  cmd[MAX_CMD_SIZE];
  int   num, avg, wjoin, wfail, wroute, t, i;
  static int time = 0;

  fscanf(fp, "%s", cmd);

  for (i = 0; i < strlen(cmd); i++) 
   cmd[i] = tolower(cmd[i]);

  if (strcmp(cmd, "events") == 0) {
    fscanf(fp, "%d %d %d %d %d", &num, &avg, &wjoin, &wfail, &wroute);
    events(num, avg, wjoin, wfail, wroute, &time);
  } else if (strcmp(cmd, "wait") == 0) {
    fscanf(fp, "%d", &t);
    time += t;
  } else if (strcmp(cmd, "exit") == 0) {
    printf("exit %d\n", time);
  } else {
    printf("command \"%s\" not known!\n", cmd);
    panic("");
  }

  fscanf(fp, "\n");
}


void ignoreCommentLineGen(FILE *fp)
{
  char ch;

  while ((ch = getc(fp)) != EOL);
}


chordID getNodeGen()
{
  int i;
  int idx = unifRand(0, NumNodes);

  for (i = 0; i < MAX_NUM_NODES; i++) {
    if ((!idx) && (ID_present[i]))
      return ID_array[i];
    if (ID_present[i])
      idx--;
  }
  panic("node out of range\n");
  {
    chordID dummy_id;
    return dummy_id; // to make the compiler happy 
                     // (otherwise we get warning) ...
  }
}
  

chordID insertNodeGen()
{
  int i, flag;
  chordID id;

  do {
    id = rand_ID();

    flag = FALSE;
    for (i = 0; i < MAX_NUM_NODES; i++) {
      if (ID_present[i] && equals(&id, &ID_array[i])) {
	/* node already present */
	flag = TRUE;
	break;
      }
    }
  } while (flag);


  /* insert node */
  for (i = 0; i  < MAX_NUM_NODES; i++) {
    if (ID_present[i] == FALSE) {
      ID_array[i] = id;
      ID_present[i] = TRUE;
      NumNodes++;
      return id;
    }
  }

  panic("no more room in Nodes table\n");
  return id;
}
  
void deleteNodeGen(chordID id)
{
  
  int i;

  for (i = 0; i < MAX_NUM_NODES; i++) {
    if (equals(&ID_array[i], &id)) {
      ID_present[i] = FALSE;
      NumNodes--;
      return;
    }
  }
  panic("deleteNodeGen: Node not found!\n");
}
    
void events(int numEvents, int avgEventInt, 
	    int wJoin, int wFail, int wRoute, int *time)
{
  int op, i, k;
  chordID id, id_target;

  // use lotery scheduling to generate join, leave, fail, insert, 
  // and find events. Each event type is associated a weight.
  // Various even types are generated according to this weight,
  // For example, out of a total of num events, 
  // num*wJoin/(wJoin+wLeave+wFail+wInsertDoc+wFindDoc) are
  // join events

  for (i = 0; i < numEvents; i++) {
    op = unifRand(0, wJoin + wFail + wRoute);

    *time += intExp(avgEventInt);
   
    if (op < wJoin) {
      printf ("join ");
      id = insertNodeGen();
      print_chordID(&id);
      printf(" %d\n", *time);
    } else if (op < wJoin + wFail) {
      id = getNodeGen();
      printf ("fail ");
      print_chordID(&id);
      printf(" %d\n", *time);
      deleteNodeGen(id);
    } else if (op < wJoin + wFail + wRoute) {
      id_target = rand_ID();
      for (k = 0; k < NUM_DUPLICATE_ROUTES; k++) {
	id = getNodeGen();
	printf ("route ");
	print_chordID(&id);
	printf(" ");
	print_chordID(&id_target);
	printf(" %d\n", *time);
      }
    } 
  }
}


/* ugly hack -- just to make it compile; needed by print_current_time in 
 * chord/util.c (sim_get_time is implemented in server.c
 */
double sim_get_time()
{
  double dummy;
  return dummy;
}

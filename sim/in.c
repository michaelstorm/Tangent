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
#include <string.h>

#include "incl.h"


void ignoreCommentLine(FILE *fp)
{
  char ch;

  while ((ch = getc(fp)) != EOL);
}

void readLine(FILE *fp)
{
  char  cmd[MAX_CMD_SIZE], srv_id[4*ID_LEN], target_id[4*ID_LEN];
  int   i, time;
  Server *srv;
  chordID id;

  fscanf(fp, "%s", cmd);

  for (i = 0; i < strlen(cmd); i++) 
   cmd[i] = tolower(cmd[i]);

  if (strcmp(cmd, "join") == 0) {
    int idx;

    fscanf(fp, "%s %d", srv_id, &time);
    id = atoid(srv_id);
    idx = add_server(&id, SRV_TO_JOIN);
    genEvent(idx, sim_join, (void *)srv_id, time);

  } else if (strcmp(cmd, "fail") == 0) {

    fscanf(fp, "%s %d", srv_id, &time);
    
    id = atoid(srv_id);
    if ((srv = get_server_by_id(&id)) != NULL)
      genEvent(srv->node.addr, sim_fail, (void *)NULL, time);
    else {
      printf("fail error: server ");
      id = atoid(srv_id);
      print_chordID(&id);
      printf(" is not available\n");
      exit (-1);
    } 

  } else if (strcmp(cmd, "route") == 0) {

    fscanf(fp, "%s %s %d", srv_id, target_id, &time);

    id = atoid(srv_id);
    if ((srv = get_server_by_id(&id)) != NULL) {
      chordID *pid;
      if ((pid = malloc(sizeof(chordID))) == NULL)
	panic("readLine: memory allocation error\n");
      *pid = atoid(target_id);
      genEvent(srv->node.addr, sim_send_data, (void *)pid, time);
    } else {
      printf("route error: server ");
      id = atoid(srv_id);
      print_chordID(&id);
      printf(" is not available\n");
      exit (-1);
    } 
  } else if (strcmp(cmd, "exit") == 0) {
    fscanf(fp, "%d", &time);
    genEvent(0, exitSim, NULL, time);
  } else {
    printf("command %s not recognized\n", cmd);
    exit(-1);
  } 

  fscanf(fp, "\n");
}

void readInputFile(char *file)
{
  FILE *fp;
  char ch;

  if ((fp = fopen(file, "r")) == NULL) {
    printf("%s: file open error.\n", file);
    exit(-1);
  }

  while (!feof(fp)) {
   if ((ch = getc(fp)) == EOL)
      continue;
    if (ch == '#') {
      ignoreCommentLine(fp);
      continue;
    } else {
      if (feof(fp))
	break;
      ungetc(ch, fp);
    }
    readLine(fp);
  }
}

  

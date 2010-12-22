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

#ifndef INCL_FUN
#define INCL_FUN

/* functions implemented by sim.c */
void exitSim(void);

/* functions implemented by node.c */
void init_global_data();
int get_server_status(int idx);
Server *get_server(int idx);
Server *get_server_by_id(chordID *id);
//Server *get_random_server();
int add_server(chordID *id, int status);

void sim_join(Server *srv, chordID *id);
void sim_fail(Server *srv);
void sim_stabilize(Server *srv);
void sim_recv_message(Server *srv, uchar *sp);
//void sim_deliver_data(Server *srv, chordID *id, int n, uchar *data);
void sim_send_data(Server *srv, chordID *id);

/* functions implemented by event.c */
void genEvent(int nodeType, void (*fun)(), void *params, double time);
Event *getEvent(CalQueue *evCal, double time);
Event **initEventQueue();
void removeEvent(CalQueue *evCal, Event *ev);

/* functions implemented by in.c */
void readInputFile(char *file);

#endif /* INC_FUN */

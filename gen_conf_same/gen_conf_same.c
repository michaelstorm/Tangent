#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "chord.h"

int main(int argc, char **argv)
{
  int i, j, k, n;
  Node *nodes;
  in6_addr v6addr;
  in6_addr v4addr;
  char dirname[100];
  char filename[100];
  FILE *fp;

  if (argc != 3) {
	fprintf(stderr, "usage: %s base_name num_servers\n", argv[0]);
	exit(-1);
  }
  char *dir = argv[1];
  n = atoi(argv[2]);
  srandom(getpid() ^ time(0));

  resolve_v6name("::1", &v6addr);
  resolve_v6name("::ffff:127.0.0.1", &v4addr);

  nodes = (Node *)malloc(n * sizeof(Node));
  for (i = 0; i < n; i++) {
	get_address_id(&nodes[i].id, &v6addr, 6500+i*2);
	nodes[i].port = 6500 + i*2;
  }

  struct stat st;
  if (stat(dir, &st) != 0)
	mkdir(dir, 0777);

  for (i = 0; i < n; i++) {
	sprintf(dirname, "%s/%d", dir, i);
	if (stat(dirname, &st) != 0)
	  mkdir(dirname, 0777);

	sprintf(filename, "%s/6.conf", dirname);
	fp = fopen(filename, "w");
	fprintf(fp, "%d\n", 6);
	fprintf(fp, "%d ", nodes[i].port);
	print_chordID(fp, &nodes[i].id);
	fprintf(fp, "\n");

	j = i-5;
	if (j < 0)
	  j = 0;

	k = i;
	if (k < 5)
	  k = 5;
	if (n < 5)
	  k = n;

	for (; j < k; j++) {
	  if (j == i) continue;
	  fprintf(fp, "[::1]:%d\n", nodes[j].port);
	}

	fclose(fp);
  }

  for (i = 0; i < n; i++) {
	get_address_id(&nodes[i].id, &v4addr, 6500+i*2+1);
	nodes[i].port = 6500 + i*2 + 1;
  }

  for (i = 0; i < n; i++) {
	sprintf(dirname, "%s/%d", dir, i);
	if (stat(dirname, &st) != 0)
	  mkdir(dirname, 0777);

	sprintf(filename, "%s/4.conf", dirname);
	fp = fopen(filename, "w");
	fprintf(fp, "%d\n", 4);
	fprintf(fp, "%d ", nodes[i].port);
	print_chordID(fp, &nodes[i].id);
	fprintf(fp, "\n");

	j = i-5;
	if (j < 0)
	  j = 0;

	k = i;
	if (k < 5)
	  k = 5;
	if (n < 5)
	  k = n;

	for (; j < k; j++) {
	  if (j == i) continue;
	  fprintf(fp, "127.0.0.1:%d\n", nodes[j].port);
	}

	fclose(fp);
  }

  return 0;
}

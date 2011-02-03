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
  in6_addr addr;
  char dirname[100];
  char filename[100];
  FILE *fp;
  struct in_addr ia;

  if (argc != 3) {
	fprintf(stderr, "usage: %s base_name num_servers\n", argv[0]);
	exit(-1);
  }
  char *dir = argv[1];
  n = atoi(argv[2]);
  srandom(getpid() ^ time(0));

  to_v6addr(get_addr(), &addr);

  nodes = (Node *)malloc(n * sizeof(Node));
  for (i = 0; i < n; i++) {
	get_address_id(&nodes[i].id, &addr, 6500+i);
	nodes[i].addr = addr;
	nodes[i].port = 6500 + i;
  }

  struct stat st;
  if (stat(dir, &st) != 0)
	mkdir(dir, 0777);

  for (i = 0; i < n; i++) {
	sprintf(dirname, "%s/%d", dir, i);
	if (stat(dirname, &st) != 0)
	  mkdir(dirname, 0777);

	sprintf(filename, "%s/conf", dirname);
	fp = fopen(filename, "w");
	fprintf(fp, "%d ", nodes[i].port);
	print_id(fp, &nodes[i].id);
	fprintf(fp, "\n");

	char key_name[100];
	sprintf(key_name, "%s/key.txt", dirname);
	FILE *key = fopen(key_name, "w");
	print_id(key, &nodes[i].id);
	fclose(key);

	j = i-5;
	if (j < 0)
	  j = 0;

	k = i;
	if (k < 5)
	  k = 5;

	for (; j < k; j++) {
	  if (j == i) continue;
	  fprintf(fp, "[%s]:%d\n", v6addr_to_str(&nodes[j].addr), nodes[j].port);
	}

	fclose(fp);
  }

  return 0;
}

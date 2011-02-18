#ifndef DHASH_H
#define DHASH_H

typedef struct
{
	char *files_path;
	int sock;
} DHash;

DHash *new_dhash(const char *files_path, const char *conf_path);

#endif

#include <dirent.h>
#include <stdio.h>
#include "dhash.h"

DHash *new_dhash(const char *files_path, const char *conf_path)
{
	DIR *dir;
	struct dirent *ent;
	dir = opendir(files_path);
	if (dir != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir(dir)) != NULL)
			printf("%s\n", ent->d_name);
		closedir(dir);
	} else {
		/* could not open directory */
		perror("");
		return NULL;
	}

	return 0; // FIX
}

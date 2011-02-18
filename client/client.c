#include <sys/wait.h>
#include "chord_api.h"
#include "dhash.h"

#include <stdlib.h>

int main(int argc, char **argv)
{
	//new_dhash("/home/michael/school/islam", 0);
	chord_init(argv+1, argc-1);
	wait(0);

	return 0;
}

#include <sys/wait.h>
#include "chord_api.h"
#include "dhash.h"

int main(int argc, char **argv)
{
	//new_dhash(0, "/home/michael/school/islam");
	chord_init(argv[1]);
	wait(0);

	return 0;
}

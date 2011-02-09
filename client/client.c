#include <sys/wait.h>
#include "chord_api.h"

int main(int argc, char **argv)
{
	int sockets[2];
	chord_init(argv[1], sockets);
	wait(0);

	return 0;
}

#include <stdio.h>
#include <unistd.h> /*sleep*/

#define INTERVAL 2
#define MAX_FAILURE 3

#include "watchdog.h"

int main(int argc, char *argv[])
{
	size_t x = 0;

	MakeMeImmortal(argc, argv, INTERVAL, MAX_FAILURE);

	printf("Running...\n");

	/*100000000000000*/
	while(x < 100000000000000)
	{
		++x;
	}

	/*StopWD();*/

	printf("stopped...\n");

	return 0;
}
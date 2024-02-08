#define _POSIX_C_SOURCE 200112L

#include <semaphore.h> /*sem_t, sem_open, sem_post*/
#include <stddef.h> /*size_t*/
#include <assert.h> /*assert*/
#include <stdio.h> /*fprintf*/
#include <stdlib.h> /*atoi, setenv*/
#include <sys/wait.h> /*pid*/
#include <signal.h> /*signal*/
#include <unistd.h> /*getpid()*/

#include "keep_watching.h"

void RunWatchDog(int argc, char *argv[])
{
	sem_t *sem = NULL;
	char pid_to_string[10];

	/*assert*/
	assert(NULL != argv);

	sem = sem_open("sem_watch", 0);
	
	/*sem_post*/
	sem_post(sem);

	sprintf(pid_to_string, "%d", getpid());

	setenv("WD_PID", pid_to_string, 1);
	
	/*KeepWatching(program_name, pid, interval, max_signals_until_crash)*/
	KeepWatching(argc, argv);
}

int main(int argc, char* argv[])
{
	/*RunWatchDog(argv[1], argv[2], atoi(argv[3]), (size_t)atoi(argv[4]), (size_t)atoi(argv[5]));*/
	RunWatchDog(argc, argv);

	return 0;
}
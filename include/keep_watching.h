#ifndef __KEEPWATCHING_H__
#define __KEEPWATCHING_H__

enum ERRORS
{
	NO_ERR,
	MALLOC_ERR,
	SEM_ERR,
	FORK_ERR,
	EXEC_ERR,
	BLOCK_SIGNALS_ERR,
	THREAD_ERR,
	SIGNAL_ERR
};

/*
	Description: Send a signal each interval to keep intouch of a specific process to make sure it always working

	Input:  argv - contain name of execute file, file to keep watching, interval, max_failures, pid of process

	Return: 0 - success, otherwise - specific fail status
*/
int KeepWatching(int argc, char *argv[]);

#endif /*__KEEPWATCHING_H__*/
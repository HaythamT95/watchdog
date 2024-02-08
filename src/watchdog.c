#define _POSIX_C_SOURCE 200112L
#include <semaphore.h> /*sem_t, sem_open, sem_post*/
#include <stddef.h> /*size_t*/
#include <assert.h> /*assert*/
#include <stdio.h> /*fprintf*/
#include <sys/wait.h> /*pid*/
#include <signal.h> /*signal, pthread_sigmask, sigaddset, sigemptyset*/
#include <stdlib.h> /*kill, malloc, setenv, getenv*/
#include <fcntl.h> /*O_CREAT*/
#include <unistd.h> /*fork, execl*/
#include <time.h> /*clock_gettime*/
#include <pthread.h> /*pthread_create*/
#include <string.h> /*strcmp*/
#include <errno.h> /*EEXIST*/

#include "watchdog.h"
#include "keep_watching.h"
#include "utils.h"

#define INTERVAL 1
#define MAX_SIGNALS 5
#define SIZE_OF_LOCALS 20

pthread_t thread;
sem_t *sem = NULL;

typedef struct watchdog_client
{
	char program_name[SIZE_OF_LOCALS];
	pid_t pid;
	size_t interval;
	size_t max_failure;
	char client_program_name[SIZE_OF_LOCALS];
	int argc;
	char** args;
}watchdog_client_ty;

void* KeepWatchingIMP(void* params);
static int SemInit(sigset_t* block_set);
static char** InitWatchDogParams(char* program_name, size_t interval, size_t max_failure, pid_t pid, int argc, char* argv[]);
static int InitClientParams(int argc, char *argv[], pid_t watchdog_pid, size_t interval, size_t max_failure, sigset_t* block_set);

int MakeMeImmortal(int argc, char *argv[], size_t interval, size_t max_failure)
{
	char* env = NULL;
	pid_t watchdog_pid = 0;
	char pid_to_string[SIZE_OF_LOCALS];
	struct timespec ts;
	sigset_t block_set;
	char** exec_args;
	int err_code = 0;

	/*assert*/
	assert(NULL != argv);
	
	/*block main thread signals*/
    if(NO_ERR != SemInit(&block_set))
    {
    	perror("sem init err");
    	return SEM_ERR;
    }

    sprintf(pid_to_string, "%d", getpid());

    setenv("CLIENT_PID", pid_to_string, 1);

	env = getenv("WD_PID");

	/*if WD_PID is NULL*/
	if(NULL == env)
	{
		/*fork() - Create watchdog process*/
	    if ((watchdog_pid = fork()) < 0)
	    {
			/*handle failure*/
	        perror("fork");
	        return FORK_ERR;
	    }
		/*if child*/
		if(0 == watchdog_pid)
		{
			/*init watchdog params*/
			exec_args = InitWatchDogParams("run_watchdog" ,interval, max_failure, getppid(), argc, argv);

			if(NULL == exec_args)
			{
				fprintf(stderr, "MALLOC_ERROR\n");
				return MALLOC_ERR;
			}
			
			/*execute watchdog program*/
			if(-1 == execv("run_watchdog", exec_args))
			{
				/*handle failure*/
				fprintf(stderr, "Couldn't Run Watchdog\n");
				return EXEC_ERR;
			}
		}
		else
		{			
			/*set WD_PID*/
		    sprintf(pid_to_string, "%d", watchdog_pid);
		    setenv("WD_PID", pid_to_string, 1);

		    clock_gettime(CLOCK_REALTIME, &ts);
    		ts.tv_sec += max_failure;

    		/*sem_timedwait*/
    		if(-1 == sem_timedwait(sem, &ts))
			{
				fprintf(stderr, "Semaphore failed\n");
				return SEM_ERR;
			}
		}
	}
	else
	{
		watchdog_pid = atoi(env);
	}

	if(NO_ERR != (err_code = InitClientParams(argc, argv, watchdog_pid, interval, max_failure, &block_set)))
	{
		return err_code;
	}

	/*return success*/
	return NO_ERR;
}

int StopWD(void)
{
	int wd_pid = 0;
	int status;
	char* env = getenv("WD_PID");
	
	wd_pid = atoi(env);
	DEBUG_ONLY(printf("killing pid = %d\n", wd_pid));

	/*Send signal(SIGUSR2) to watchdog*/
	if(-1 == kill(wd_pid, SIGUSR2))
	{
		perror("Error signaling process");
		return SIGNAL_ERR;
	}

	/*verify kill*/
	if(-1 == waitpid(wd_pid, &status, 0))
	{
		perror("Error waiting for process");
		return SIGNAL_ERR;
	}

	/*return success*/
	return NO_ERR;
}

void* KeepWatchingIMP(void* params)
{
	watchdog_client_ty *watchdog = (watchdog_client_ty*)params;
	char **exec_args = (char**)malloc((watchdog->argc + 6) * sizeof(char*));
	char pid_to_string[SIZE_OF_LOCALS];
	char ppid_to_string[SIZE_OF_LOCALS];
	char interval_str[SIZE_OF_LOCALS];
	char max_signals_str[SIZE_OF_LOCALS];
	char argc_to_string[SIZE_OF_LOCALS];
	size_t i = 0;

	sprintf(interval_str, "%lu", watchdog->interval);
	sprintf(max_signals_str, "%lu", watchdog->max_failure);
	sprintf(pid_to_string, "%d", watchdog->pid);
	sprintf(argc_to_string, "%d", watchdog->argc);

	exec_args[0] = watchdog->client_program_name;
	exec_args[1] = interval_str;
	exec_args[2] = max_signals_str;
	exec_args[3] = pid_to_string;
	exec_args[4] = argc_to_string;
	exec_args[5] = "run_watchdog";

	for ( i = 1; i < watchdog->argc; ++i)
	{
		exec_args[i + 5] = watchdog->args[i];
	}

	KeepWatching(watchdog->argc, exec_args);

	return NULL;
}

int SemInit(sigset_t* block_set)
{
	sem_unlink("sem_watch");

	/*set named semaphore*/
	sem = sem_open("sem_watch", O_CREAT , 0666, 0);

	if(SEM_FAILED == sem)
    {
        if(EEXIST == errno)
        {
            sem = sem_open("sem_watch", 0);
        }
        else
        {
            sem_unlink("sem_watch");
            return SEM_ERR;
        }
    }

    /*block signals*/
	if (-1 == sigemptyset(block_set)) 
	{
        perror("sigemptyset");
        return BLOCK_SIGNALS_ERR;
    }

    if (-1 == sigaddset(block_set, SIGUSR1) || -1 == sigaddset(block_set, SIGUSR2) || -1 == sigaddset(block_set, SIGKILL))
    {
        perror("sigaddset");
        return BLOCK_SIGNALS_ERR;
    }

    if (-1 == pthread_sigmask(SIG_BLOCK, block_set, NULL)) 
    {
        perror("sigprocmask");
        return BLOCK_SIGNALS_ERR;
    }
    return NO_ERR;
}

char** InitWatchDogParams(char* program_name, size_t interval, size_t max_failure, pid_t pid, int argc, char* argv[])
{
	size_t i = 0;
	char pid_to_string[SIZE_OF_LOCALS];
	char ppid_to_string[SIZE_OF_LOCALS];
	char interval_str[SIZE_OF_LOCALS];
	char max_signals_str[SIZE_OF_LOCALS];
	char argc_to_string[SIZE_OF_LOCALS];
	char** exec_args;

	assert(NULL != program_name);

	sprintf(interval_str, "%lu", interval);
	sprintf(max_signals_str, "%lu", max_failure);
	sprintf(ppid_to_string, "%d", getppid());
	sprintf(argc_to_string, "%d", argc);

	exec_args = (char**)malloc((argc + 6) * sizeof(char*));

	if(NULL == exec_args)
	{
		fprintf(stderr, "MALLOC_ERROR\n");
		return NULL;
	}

	exec_args[0] = program_name;
	exec_args[1] = interval_str;
	exec_args[2] = max_signals_str;
	exec_args[3] = ppid_to_string;
	exec_args[4] = argc_to_string;

	for (i = 5; i < argc + 5; ++i) 
	{
	    exec_args[i] = argv[i - 5];
	}
	exec_args[i] = NULL;

	return exec_args;
}

int InitClientParams(int argc, char *argv[], pid_t watchdog_pid, size_t interval, size_t max_failure, sigset_t* block_set)
{
	watchdog_client_ty *wd_client = NULL;

	/*assert*/
	assert(NULL != argv);

	/*init client params*/
	wd_client = (watchdog_client_ty*)malloc(sizeof(watchdog_client_ty));

	if(NULL == wd_client)
	{
		fprintf(stderr, "MALLOC_ERROR\n");
		return MALLOC_ERR;
	}

	strcpy(wd_client->program_name, "run_watchdog");
	wd_client->pid = watchdog_pid;
	wd_client->interval = interval;
	wd_client->max_failure = max_failure;
	strcpy(wd_client->client_program_name, argv[0]);
	wd_client->args = argv;
	wd_client->argc = argc;

	/*Create Thread*/
		/*Call KeepWatching(program_name, pid, interval, max_failure)*/
	if(0 != pthread_create(&thread, NULL, KeepWatchingIMP, wd_client))
	{
		fprintf(stderr, "Thread creation failed\n");
		return THREAD_ERR;
	}

	if (-1 == pthread_sigmask(SIG_UNBLOCK, block_set, NULL))
	{
	    perror("sigprocmask");
	    return BLOCK_SIGNALS_ERR;
	}
	return NO_ERR;
}
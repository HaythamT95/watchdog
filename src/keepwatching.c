#define _POSIX_C_SOURCE 200112L
#include <sys/wait.h> /*pid*/
#include <stddef.h> /*size_t*/
#include <assert.h> /*assert*/
#include <signal.h> /*signal, pthread_sigmask, sigaddset, sigemptyset*/
#include <stdlib.h> /*kill, getenv, setenv, malloc, unsetenv*/
#include <string.h> /*strcmp*/
#include <stdio.h> /*fprintf*/
#include <semaphore.h> /*sem_unlink*/
#include <unistd.h> /*fork, execl*/

#include "scheduler.h"
#include "keep_watching.h"
#include "utils.h"

#define SIZE_OF_LOCALS 20
#define SENDING_SIGNAL_INTERVAL 2

typedef struct watchdog
{
	char *program_name;
	pid_t pid;
	size_t interval;
	size_t max_failure;
	int argc;
	char *client_program_name;
	char** args;
}watchdog_ty;

static void ResetCounter(int signo);
static void StopSignal(int signo);
int KeepWatching(int argc, char *argv[]);
static watchdog_ty InitFields(char *program_name, pid_t pid, size_t interval, size_t max_failure, int num_of_args, char *argv[]);
static int CheckSignals(void* params);
static int SendSignal(void* params);
static void Revive(void* params);
static void CleanUp();
static int BlockSignals(sigset_t* blockSet);

sched_ty *scheduler = NULL;
size_t g_num_of_signals = 0;
int g_stop_signal = 0;
watchdog_ty watchdog;

int KeepWatching(int argc, char *argv[])
{
	sigset_t blockSet;
	struct sigaction sa, sa2;
    sa.sa_handler = ResetCounter;
    sa2.sa_handler = StopSignal;
    sa.sa_flags = 0;
    sa2.sa_flags = 0;
    char* program_name = NULL;
    pid_t pid;
    size_t interval = 0;
    size_t max_failure = 0;
    int num_of_args = 0;

	/*assert*/
	assert(NULL != argv);

	program_name = argv[5];
	pid = atoi(argv[3]);
	interval = atoi(argv[1]);
	max_failure = atoi(argv[2]);
	num_of_args = atoi(argv[4]);

	watchdog = InitFields(program_name, pid, interval, max_failure, num_of_args, argv);

	/*block signals*/
	if(NO_ERR != BlockSignals(&blockSet))
	{
		perror("Couldn't block signals\n");
		return BLOCK_SIGNALS_ERR;
	}

	/*receive sig1*/
	sigaction(SIGUSR1, &sa, NULL);

	/*receive sig2*/
    sigaction(SIGUSR2, &sa2, NULL);

	/*Create scheduler*/
	scheduler = SchedCreate();

	if(NULL == scheduler)
	{
		fprintf(stderr, "Couldn't Create Scheduler\n");
		return MALLOC_ERR;
	}

	/*Init task*/
		/*SendSignal()*/
	SchedAddTask(scheduler, SendSignal, (void*)&watchdog, SENDING_SIGNAL_INTERVAL);
		/*CheckSignals()*/
	SchedAddTask(scheduler, CheckSignals, (void*)&watchdog, interval);

	/*Run scheduler*/
	SchedRun(scheduler);
}

void ResetCounter(int signo)
{
	(void)signo;
	/*reset cnt*/
	__sync_lock_test_and_set(&g_num_of_signals, 0);
}

void StopSignal(int signo)
{
	/*printf("dying pid = %d\n", getpid());*/
	(void)signo;
	/*stop_signal = 1*/
	__sync_lock_test_and_set(&g_stop_signal, 1);
}

int CheckSignals(void* params)
{
	watchdog_ty watchdog;
	size_t max_failure = 0;

	assert(NULL != params);

	watchdog = *(watchdog_ty*)params;
	max_failure = watchdog.max_failure;

	DEBUG_ONLY(printf("checking pid = %d\n", getpid()));

	/*if stop_signal*/
	if(g_stop_signal)
	{
		/*CleanUp()*/
		CleanUp();
	}

	/*if cnt > max_failure*/
	if(g_num_of_signals > max_failure)
	{
		/*reset signals*/
		__sync_lock_test_and_set(&g_num_of_signals, 0);

		/*Revive()*/
		Revive(params);
	}

	return 1;
}

int SendSignal(void* params)
{
	watchdog_ty watchdog;
	pid_t pid;

	assert(NULL != params);

	watchdog = *(watchdog_ty*)params;
	pid = watchdog.pid;

	DEBUG_ONLY(printf("sending pid = %d\n", getpid()));

	/*++cnt - atomic*/
	__sync_fetch_and_add(&g_num_of_signals, 1);
	/*send signal(SIGUSR1)*/
	kill(pid, SIGUSR1);

	return 1;
}

void Revive(void* params)
{
	int status;
	pid_t new_pid_watchdog = 0;
	pid_t pid = 0;
	char pid_to_string[SIZE_OF_LOCALS];
	char ppid_to_string[SIZE_OF_LOCALS];
	char interval_str[SIZE_OF_LOCALS];
	char max_signals_str[SIZE_OF_LOCALS];
	watchdog_ty watchdog_rev;
	size_t i = 0;

	assert(NULL != params);

	watchdog_rev = *(watchdog_ty*)params;

	pid = watchdog_rev.pid;

	/*kill process*/
	if (-1 == kill(pid, SIGKILL)) 
	{
        perror("Error killing process");
    }

    /*wait until its killed*/
    if (-1 == waitpid(pid, &status, 0))
    {
        perror("Error waiting for process");
    }

	/*clients need to run watch*/
	if(0 == strcmp(watchdog_rev.program_name, "run_watchdog"))
	{
		DEBUG_ONLY(printf("client executing dog\n"));
		if ((new_pid_watchdog = fork()) < 0)
	    {
	        perror("fork");
	        exit(1);
	    }
	    if(0 == new_pid_watchdog)
	    {
	    	/*init watchdog params*/
	    	sprintf(interval_str, "%lu", watchdog.interval);
			sprintf(max_signals_str, "%lu", watchdog.max_failure);
			char *env_client = getenv("CLIENT_PID");

			char** execArguments = (char**)malloc((watchdog.argc + 6) * sizeof(char*));

			execArguments[0] = "run_watchdog";
			execArguments[1] = interval_str;
		    execArguments[2] = max_signals_str;
		    execArguments[3] = env_client;
		    execArguments[4] = "0";
		    execArguments[5] = watchdog.client_program_name;

		    for ( i = 6; i < watchdog.argc + 6; ++i)
		    {
		    	 execArguments[i] = watchdog.args[i - 5];
		    }
		    execArguments[i] = NULL;


			/*Fork & Execute program*/
			if (-1 == execv("run_watchdog", execArguments))
			{
				fprintf(stderr, "Couldn't Run Watchdog\n");
				return;
			}
	    }
	    else
	    {
	    	sprintf(pid_to_string, "%d", new_pid_watchdog);
	    	setenv("WD_PID", pid_to_string, 1);
	    	watchdog.pid = new_pid_watchdog;
	    }
	}
	/*watchdog need to run client*/
	else
	{
		DEBUG_ONLY(printf("watching executing client\n"));
		if ((new_pid_watchdog = fork()) < 0)
	    {
	        perror("fork");
	        exit(1);
	    }
	    if(0 == new_pid_watchdog)
	    {
			/*Fork & Execute program*/
			if (-1 == execv(watchdog.program_name, watchdog.args))
			{
				fprintf(stderr, "Couldn't Run Watchdog\n");
				return;
			}
	    }
	    else
	    {
	    	watchdog.pid = new_pid_watchdog;
	    }
	    wait(&status);
	}
}

void CleanUp()
{
	DEBUG_ONLY(printf("cleaning pid = %d\n", getpid()));

	/*Destroy sched*/
	SchedDestroy(scheduler);

	/*Destroy sems*/
	if(-1 == sem_unlink("sem_watch"))
	{
		fprintf(stderr, "Couldn't Unlink semaphore\n");
	}

	/*Remove ENV var*/
	if(-1 == unsetenv("WD_PID"))
	{
		fprintf(stderr, "Couldn't unset env variable\n");
	}
	kill(getpid(), SIGKILL);
}

watchdog_ty InitFields(char *program_name, pid_t pid, size_t interval, size_t max_failure, int num_of_args, char *argv[])
{
	watchdog_ty watchdog;
	size_t i = 0;

	watchdog.program_name = program_name;
	watchdog.pid = pid;
	watchdog.interval = interval;
	watchdog.max_failure = max_failure;
	watchdog.argc = num_of_args;
	watchdog.client_program_name = argv[0];

	watchdog.args = (char**)malloc(sizeof(char*) * (watchdog.argc + 1));

	for ( i = 0; i < watchdog.argc; ++i)
	{
		watchdog.args[i] = argv[i + 5];
	}
	watchdog.args[i] = NULL;

	return watchdog;
}

int BlockSignals(sigset_t* blockSet)
{
	if (-1 == sigemptyset(blockSet)) 
	{
        perror("sigemptyset");
        return BLOCK_SIGNALS_ERR;;
    }

    if (-1 == sigaddset(blockSet, SIGUSR1) || -1 == sigaddset(blockSet, SIGUSR2) || -1 == sigaddset(blockSet, SIGKILL))
    {
        perror("sigaddset");
        return BLOCK_SIGNALS_ERR;
    }

    if (-1 == pthread_sigmask(SIG_UNBLOCK, blockSet, NULL)) 
    {
        perror("sigprocmask");
        return BLOCK_SIGNALS_ERR;
    }
    return NO_ERR;
}
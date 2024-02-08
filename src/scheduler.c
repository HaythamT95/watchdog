/*******************************************************************************
1)Create a New Scheduler (SchedCreate):
*Generate a fresh scheduler by allocating memory for it.
*Allocate memory for the priority queue.
*Initialize any other necessary details.
*Receive a reference to the newly created scheduler.

2)Destroy a Scheduler (SchedDestroy):
*Check if theres an existing scheduler to be removed.
*If so, dispose of the priority queue.
*Release the memory allocated for the scheduler.

4)Add a Task to the Scheduler (SchedAddTask):
*Ensure the input parameters are valid.
*Formulate a new task.
*Insert the task into the priority queue.
*Provide the unique ID (UID) associated with the added task.

5)Remove a Task from the Scheduler by UID (SchedRemoveTask):
*Validate the correctness of the input parameters.
*Extract the specified task from the priority queue.
*Report the success or failure of the removal.

6)Stop Running Tasks in the Scheduler (SchedStop):
*erify that the input parameter is sensible.
*ndicate to the scheduler to cease running tasks.
*ommunicate the status of the operation.

7)Return the Number of Tasks in the Scheduler (SchedSize):
*Confirm the appropriateness of the input parameter.
*Determine the quantity of tasks in the priority queue.
*Convey the count of tasks.

8)Check if the Scheduler is Empty (SchedIsEmpty):
*Validate the input parameter.
*Determine whether the priority queue has any tasks.
*Respond with either True (1) or False (0).

9)Remove All Tasks from the Scheduler (SchedClear):
*Confirm the suitability of the input parameter.
*Eliminate all tasks from the priority queue.
*Communicate the status of the operation.
*******************************************************************************/

#include<time.h>/*time*/
#include <assert.h>/*assert*/
#include <stdio.h>/*fprintf*/
#include <stddef.h> /*size_t*/
#include <stdlib.h>/*malloc free*/
#include <unistd.h>/*sleep*/
#include "scheduler.h"
#include "priority.h"
#include "task.h"


enum {SCHEDULER_STOP = 1, SCHEDULER_RUN = 0};

struct scheduler 
{
	priority_ty *p_queue;
	task_ty *curr_task;
	int to_stop;
};

sched_ty *SchedCreate()
{
    sched_ty *sched = (sched_ty *)malloc(sizeof(sched_ty));

    if (sched == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for scheduler\n");
        return NULL;
    }

    sched->p_queue = PriorityCreate(TaskIsBefore);

    if (sched->p_queue == NULL)
    {
        fprintf(stderr, "Failed to create priority queue\n");
        free(sched);
        return NULL;
    }

    sched->curr_task = NULL;
    sched->to_stop = TASK_NO_REPEAT;

    return sched;
}

void SchedDestroy(sched_ty *scheduler)
{
   assert(scheduler);

    SchedClear(scheduler);
    PriorityDestroy(scheduler->p_queue);
    scheduler->p_queue = NULL;
    free(scheduler);
    scheduler = NULL; 
}

uid_ty SchedAddTask(sched_ty *sched, sched_op_func_ty op_func,
                    void *op_params, time_t interval_in_sec)
{
    task_ty *task = NULL;
    
    sched_status_ty status = SCHED_NO_ERROR;
    uid_ty task_id = UIDGetBadUID();

    assert(NULL != sched);
	assert(NULL != op_func);
	assert(NULL != op_params);
	assert(0 < interval_in_sec);

    task = TaskCreate(interval_in_sec, op_func, op_params);

    if (task == NULL)
    {
        fprintf(stderr, "Failed to create task\n");
        return task_id;
    }

    status = (sched_status_ty)PriorityEnqueue(sched->p_queue, task);

    if (status == SCHED_NO_ERROR)
    {
        task_id = TaskGetUID(task);
        sched->curr_task = PriorityPeek(sched->p_queue);
    }
    else
    {
        fprintf(stderr, "Failed to enqueue task\n");
        TaskDestroy(task);
    }

    return task_id;
}


sched_status_ty SchedRemoveTask(sched_ty *scheduler, uid_ty uid)
{
    task_ty *task = NULL;
    sched_cmp_func_ty cmp_func = TaskIsMatch;

    assert(NULL != scheduler);
    task = PriorityErase(scheduler->p_queue,cmp_func,&uid);
    TaskDestroy(task);
    task = NULL;

    return SCHED_NO_ERROR;
}
void SchedRun(sched_ty *sched)
{
    task_ty *task = NULL;
    task_repeat_status_ty task_func_status = TASK_NO_REPEAT;
    time_t time_to_sleep = 0;

	assert(NULL != sched);

    while (!sched->to_stop && !SchedIsEmpty(sched))
    {
        task = PriorityDequeue(sched->p_queue);

        if (task == NULL)
        {
            fprintf(stderr, "Failed to dequeue task\n");
            break;
        }

        time_to_sleep = TaskGetTimeToRun(task) - time(NULL);

        if (time_to_sleep > 0)
        {
            sleep(time_to_sleep);
        }

        task_func_status = TaskRun(task);

        if (task_func_status == TASK_REPEAT)
        {
            if (TaskUpdateTimeToRun(task) == TASK_ERR)
            {
                fprintf(stderr, "Task time update error\n");
            }
            else
            {
                if ((sched_status_ty)PriorityEnqueue(sched->p_queue, task) != 
                                                                SCHED_NO_ERROR)
                {
                    fprintf(stderr, "Task enqueue error\n");
                    TaskDestroy(task);
                }
            }
        }
        else if (task_func_status == TASK_REPEAT_ERR)
        {
            fprintf(stderr, "Task run error\n");
        }
        else if (task_func_status == TASK_NO_REPEAT)
        {
            TaskDestroy(task);
        }
    }
}



void SchedStop( sched_ty *sched)
{
	sched->to_stop = SCHEDULER_STOP;
}


size_t SchedSize(const sched_ty *sched)
{
	return PrioritySize(sched->p_queue);
}


int SchedIsEmpty(const sched_ty *sched)
{
	assert(NULL != sched);

	return PriorityIsEmpty(sched->p_queue);
}


void SchedClear( sched_ty *sched)
{
	task_ty *task = NULL;

	assert(NULL != sched);

	while(!SchedIsEmpty(sched))
	{
		task = PriorityDequeue(sched->p_queue);
		TaskDestroy(task);
		task = NULL;
	}
}
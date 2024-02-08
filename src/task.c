/*******************************************************************************
1)TaskCreate:
*Validate input parameters.
*Allocate memory for a new task.
*Initialize task fields.
*Return a pointer to the new task.

2)TaskDestroy:
*Validate input parameter.
*Free memory allocated for the task.

3)TaskUpdateTimeToRun:
*Validate input parameter.
*Calculate tasks priority.
*Update the priority field.
*Return the status: Succeed(0) or Failed(1).

4)TaskGetTimeToRun:
*Validate input parameter.
*Return the time_to_run of a task.

5)TaskGetUID:
*Validate input parameter.
*Return the task ID.

6)TaskIsMatch:
*Validate input parameters.
*Compare task ID with the given UID.
*Return True(1) if there is a match, False(0) otherwise.

7)TaskRun:
*Validate input parameter.
*Call the tasks operation function with the provided parameters.
*Return the status of whether to repeat the task or not.

8)TaskIsBefore:
*Validate input parameters.
*Compare the priority of the first task with the second task.
*Return True(1) if the first task should happen before the second task, False(0) otherwise.
*******************************************************************************/
#include<stdio.h>
#include<time.h>
#include <stdlib.h>
#include <assert.h>

#include"task.h"

enum {TASK_MATCH = 0, TASK_NOT_MATCH = 1};
task_ty *TaskCreate(time_t interval_in_sec, task_op_func_ty op_func, void *op_params)
{
    task_ty *task = NULL;

    assert(0 < interval_in_sec);
    assert(NULL != op_func);

    task = (task_ty *)malloc(sizeof(task_ty));

    if(NULL == task)
    {
        fprintf(stderr, "%s\n","Memory Allocation Error!" );

        return NULL;
    }

    task->task_id = UIDGenerate();
    task->op_func = op_func;
    task->op_params = op_params;
    task->interval_in_sec = interval_in_sec;
    task->time_to_run = time(NULL) + interval_in_sec;


    return task;
}


void TaskDestroy(task_ty *task)
{
    assert(NULL != task);

    free(task);

    task = NULL;
}

task_status_ty TaskUpdateTimeToRun(task_ty *task)
{
    assert(NULL != task);

    task->time_to_run = time(NULL) + task -> interval_in_sec;

    if(-1 == task->time_to_run)
    {
        return TASK_ERR;
    }

    return TASK_NO_ERR;    
}

time_t TaskGetTimeToRun(task_ty *task)
{
    assert(NULL != task);

    return task->time_to_run;
}

uid_ty TaskGetUID(task_ty *task)
{
    assert(NULL != task);

    return task->task_id;
}
int TaskIsMatch(const void *task, const void *task_id)
{
	assert(NULL != task);

	return UIDIsSame(((task_ty *)task)->task_id, *(uid_ty *)task_id) ?
												 TASK_MATCH : TASK_NOT_MATCH;
}

task_repeat_status_ty TaskRun(task_ty *task)
{
    task_repeat_status_ty status = TASK_NO_ERR;

	assert(NULL != task);

	status = task->op_func(task->op_params);

	return status;
}

int TaskIsBefore(const void *first_task, const void *second_task)
{
	int return_val = 0;
	time_t res = ((task_ty *)first_task)->time_to_run - 
											((task_ty *)second_task)->time_to_run;
	
	assert(NULL != first_task);
	assert(NULL != second_task);

	if(0 < res)
	{
		return_val = TASK_CMP_BIGGER_THAN;
	}
	if(0 > res)
	{
		return_val = TASK_CMP_SMALLER_THAN;
	}
	if(0 == res)
	{
		return_val = TASK_CMP_EQUAL;
	}

	return return_val;

}
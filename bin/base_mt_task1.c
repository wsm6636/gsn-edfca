/* based_task.c -- A basic real-time task skeleton. 
 *
 * This (by itself useless) task demos how to setup a 
 * single-threaded LITMUS^RT real-time task.
 */

/* First, we include standard headers.
 * Generally speaking, a LITMUS^RT real-time task can perform any
 * system call, etc., but no real-time guarantees can be made if a
 * system call blocks. To be on the safe side, only use I/O for debugging
 * purposes and from non-real-time sections.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
/* Second, we include the LITMUS^RT user space library header.
 * This header, part of liblitmus, provides the user space API of
 * LITMUS^RT.
 */
#include "litmus.h"

/* Next, we define period and execution cost to be constant. 
 * These are only constants for convenience in this example, they can be
 * determined at run time, e.g., from command line parameters.
 *
 * These are in milliseconds.
 */
#define PERIOD            70
#define RELATIVE_DEADLINE 70
#define EXEC_COST         30
#define PHASE		  5	
/* Catch errors.
 */
#define CALL( exp ) do { \
		int ret; \
		ret = exp; \
		if (ret != 0) \
			fprintf(stderr, "%s failed: %m\n", #exp);\
		else \
			fprintf(stderr, "%s ok.\n", #exp); \
	} while (0)


/* Declare the periodically invoked job. 
 * Returns 1 -> task should exit.
 *         0 -> task should continue.
 */
int job(void);

/* typically, main() does a couple of things: 
 * 	1) parse command line parameters, etc.
 *	2) Setup work environment.
 *	3) Setup real-time parameters.
 *	4) Transition to real-time mode.
 *	5) Invoke periodic or sporadic jobs.
 *	6) Transition to background mode.
 *	7) Clean up and exit.
 *
 * The following main() function provides the basic skeleton of a single-threaded
 * LITMUS^RT real-time task. In a real program, all the return values should be 
 * checked for errors.
 */
int main(int argc, char** argv)
{
	int do_exit;
	struct rt_task param;
	
	/* Setup task parameters */
	init_rt_task_param(&param);
	param.exec_cost = ms2ns(EXEC_COST);
	param.period = ms2ns(PERIOD);
	param.relative_deadline = ms2ns(RELATIVE_DEADLINE);
	
	/* What to do in the case of budget overruns? */
	//param.budget_policy = NO_ENFORCEMENT;

	/* The task class parameter is ignored by most plugins. */
	//param.cls = RT_CLASS_SOFT;

	/* The priority parameter is only used by fixed-priority plugins. */
	//param.priority = 2;
	param.phase  = ms2ns(PHASE);
	/* The task is in background mode upon startup. */
	param.num_cache_partitions = 1;

	/*****
	 * 1) Command line paramter parsing would be done here.
	 */



	/*****
	 * 2) Work environment (e.g., global data structures, file data, etc.) would
	 *    be setup here.
	 */



	/*****
	 * 3) Setup real-time parameters. 
	 *    In this example, we create a sporadic task that does not specify a 
	 *    target partition (and thus is intended to run under global scheduling). 
	 *    If this were to execute under a partitioned scheduler, it would be assigned
	 *    to the first partition (since partitioning is performed offline).
	 */
	CALL( init_litmus() );

	/* To specify a partition, do
	 *
	 * param.cpu = CPU;
	 * be_migrate_to(CPU);
	 *
	 * where CPU ranges from 0 to "Number of CPUs" - 1 before calling
	 * set_rt_task_param().
	 */
	CALL( set_rt_task_param(gettid(), &param) );


	/*****
	 * 4) Transition to real-time mode.
	 */
	CALL( task_mode(LITMUS_RT_TASK) );

	/* The task is now executing as a real-time task if the call didn't fail. 
	 */


	wait_for_ts_release();
	/*****
	 * 5) Invoke real-time jobs.
	 */
	do {
		/* Wait until the next job is released. */
		
		/* Invoke job. */
		
			
		do_exit = job();
		sleep_next_period();	
	} while (!do_exit);


	
	/*****
	 * 6) Transition to background mode.
	 */
	CALL( task_mode(BACKGROUND_TASK) );



	/***** 
	 * 7) Clean up, maybe print results and stats, and exit.
	 */
	return 0;
}
int rann[600000];
int a=0;
int job(void) 
{
	struct item {
        int data;
        int in_use;
        struct list_head list;
} __attribute__((aligned(32)));
	/* Do real-time calculation. */
	int g_mem_size = 1024  * 16 ;
	int workingset_size = g_mem_size / 32;
//	struct item *list;
//	struct list_head head;
	int i=0;
	a++;
	if(a>3)return 1;
	else{
	printf("a=%d\n",a);
/*        INIT_LIST_HEAD(&head);

        list = (struct item *)malloc(sizeof(struct item) * workingset_size + 32);

        for (i = 0; i < workingset_size; i++) {
                        list[i].data = i;
                        list[i].in_use = 0;
                        INIT_LIST_HEAD(&list[i].list);
                }*/

//        int *rann = (int *)malloc(workingset_size * sizeof(int));
	
        for (i = 0; i < workingset_size; i++) 
              rann[i] = i;
        for (i = 0; i < workingset_size; i++) {
                                int ntmp = rann[i];
                                int nrpo = rand() % workingset_size; 
                                rann[i] = rann[nrpo];
                                rann[nrpo] = ntmp;                        
                        }
/*
	for (i = 0; i < workingset_size; i++) {
                        list_add(&list[rann[i]].list, &head);
                        //printf("%d\n", rann[i]);
                }*/

	/* Don't exit. */
	return 0;
	}
	
}

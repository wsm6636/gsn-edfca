#ifndef LITMUS_RT_EDFCA_H
#define LITMUS_RT_EDFCA_H

#include <litmus/preempt.h>
/*
#define TRACE_EDFCA_STATE_CHANGE(x, y, task)				\
	TRACE_TASK(task, "job:%d cp_mask:0x%x %d(%s) -> %d(%s)\n",	\
		    tsk_rt(task)->job_params.job_no, 			\
			tsk_rt(task)->job_params.cache_partitions,	\
			(x), ca_state_name(x),					\
		    (y), ca_state_name(y))
*/
typedef struct  {
	int 			cpu;
	uint32_t 		used_cp; 		/* currently used cp partition */
} cpu_edfca_entry_t;

/* lock_cache_partitions
 * lock cp_mask for cpu so that only cpu can use cp_mask
 * NOTE:
 * 1) rt.lock is grabbed by the caller so that
 *    scheduler on diff CPUs do not have race condition
 * 2) We have race condition when user write to /proc/sys
 *    As long as users do not write to /proc/sys, we are safe
 */
void lock_edfca_partitions(int cpu, uint32_t cp_mask, struct task_struct *tsk, rt_domain_t *rt);

/* unlock_cache_partitions
 * unlock cp_mask for cpu so that other cpus can use cp_mask
 */
void unlock_edfca_partitions(int cpu, uint32_t cp_mask, rt_domain_t *rt);

int edfca_count_set_bits(uint32_t bitmask);

void
edfca_flush_cache_partitions(int cpu, uint16_t cp_mask);

#endif

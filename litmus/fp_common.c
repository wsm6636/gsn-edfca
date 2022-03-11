/*
 * litmus/fp_common.c
 *
 * Common functions for fixed-priority scheduler.
 */

#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/list.h>

#include <litmus/litmus.h>
#include <litmus/sched_plugin.h>
#include <litmus/sched_trace.h>

#include <litmus/fp_common.h>

/* fp_higher_prio -  returns true if first has a higher static priority
 *                   than second. Ties are broken by PID.
 *
 * both first and second may be NULL
 */
int fp_higher_prio(struct task_struct* first,
		   struct task_struct* second)
{
	struct task_struct *first_task = first;
	struct task_struct *second_task = second;

	/* There is no point in comparing a task to itself. */
	if (unlikely(first && first == second)) {
		TRACE_TASK(first,
			   "WARNING: pointless FP priority comparison.\n");
		return 0;
	}

	/* check for NULL tasks */
	if (!first || !second)
		return first && !second;

	if (!is_realtime(second_task))
		return 1;

#ifdef CONFIG_LITMUS_LOCKING

	/* Check for inherited priorities. Change task
	 * used for comparison in such a case.
	 */
	if (unlikely(first->rt_param.inh_task))
		first_task = first->rt_param.inh_task;
	if (unlikely(second->rt_param.inh_task))
		second_task = second->rt_param.inh_task;

	/* Comparisons to itself are only possible with
	 * priority inheritance when svc_preempt interrupt just
	 * before scheduling (and everything that could follow in the
	 * ready queue). Always favour the original job, as that one will just
	 * suspend itself to resolve this.
	 */
	if(first_task == second_task)
		return first_task == first;

	/* Check for priority boosting. Tie-break by start of boosting.
	 */
	if (unlikely(is_priority_boosted(first_task))) {
		/* first_task is boosted, how about second_task? */
		if (is_priority_boosted(second_task))
			/* break by priority point */
			return lt_before(get_boost_start(first_task),
					 get_boost_start(second_task));
		else
			/* priority boosting wins. */
			return 1;
	} else if (unlikely(is_priority_boosted(second_task)))
		/* second_task is boosted, first is not*/
		return 0;

#else
	/* No locks, no priority inheritance, no comparisons to itself */
	BUG_ON(first_task == second_task);
#endif

	if (get_priority(first_task) < get_priority(second_task))
		return 1;
	else if (get_priority(first_task) == get_priority(second_task))
		/* Break by PID. */
		return first_task->pid < second_task->pid;
	else
		return 0;
}

int fp_ready_order(struct bheap_node* a, struct bheap_node* b)
{
	return fp_higher_prio(bheap2task(a), bheap2task(b));
}

void fp_domain_init(rt_domain_t* rt, check_resched_needed_t resched,
		    release_jobs_t release)
{
	rt_domain_init(rt,  fp_ready_order, resched, release);
}

/* need_to_preempt - check whether the task t needs to be preempted
 */
int fp_preemption_needed(struct fp_prio_queue *q, struct task_struct *t)
{
	struct task_struct *pending;

	pending = fp_prio_peek(q);

	if (!pending)
		return 0;
	if (!t)
		return 1;

	/* make sure to get non-rt stuff out of the way */
	return !is_realtime(t) || fp_higher_prio(pending, t);
}
/* need_to_preempt - check whether the task t needs to be preempted
 *                   call only with irqs disabled and with  ready_lock acquired
 *                   THIS DOES NOT TAKE NON-PREEMPTIVE SECTIONS INTO ACCOUNT!
 * MX: copy from edf_preemption_needed
 */
int gfp_preemption_needed(rt_domain_t* rt, struct task_struct *t)
{
	/* we need the read lock for fp_ready_queue */
	/* no need to preempt if there is nothing pending */
	if (!__jobs_pending(rt))
		return 0;
	/* we need to reschedule if t doesn't exist */
	if (!t)
		return 1;

	/* NOTE: We cannot check for non-preemptibility since we
	 *       don't know what address space we're currently in.
	 */

	/* make sure to get non-rt stuff out of the way */
	return !is_realtime(t) || fp_higher_prio(__next_ready(rt), t);
}

/* need_to_preempt - check whether the task t needs to be preempted
 *                   call only with irqs disabled and with  ready_lock acquired
 *                   THIS DOES NOT TAKE NON-PREEMPTIVE SECTIONS INTO ACCOUNT!
 * MX: copy from gfp_preemption_needed
 *     do not preempt a real-time task
 */
int gnpfp_preemption_needed(rt_domain_t* rt, struct task_struct *t)
{
	/* we need the read lock for fp_ready_queue */
	/* no need to preempt if there is nothing pending */
	if (!__jobs_pending(rt))
		return 0;
	/* we need to reschedule if t doesn't exist */
	if (!t)
		return 1;

	/* NOTE: We cannot check for non-preemptibility since we
	 *       don't know what address space we're currently in.
	 */

    if (!is_realtime(t))
        return 1;
    else
        return 0;
}

int count_set_bits(uint32_t bitmask)
{
	int i = 0;
	int num_bits = 0;
	for (i = 0; i < MAX_NUM_CACHE_PARTITIONS; i++)
	{
		if (bitmask & (1 << i))
			num_bits++;
	}
	return num_bits;
}


/* need_to_preempt - check whether the task t needs to be preempted
 *                   call only with irqs disabled and with  ready_lock acquired
 *                   THIS DOES NOT TAKE NON-PREEMPTIVE SECTIONS INTO ACCOUNT!
 * MX: copy from gfp_preemption_needed
 */

//int gfpca_preemption_needed(rt_domain_t* rt, struct task_struct *t)
//{
//	/* we need the read lock for fp_ready_queue */
//	int high_priority = 0;
//	int enough_cache = 0;
//	int used_cache_partitions = 0;
//	struct task_struct *next = NULL;
//	/* no need to preempt if there is nothing pending */
//	if (!__jobs_pending(rt))
//		return 0;
//	/* we need to reschedule if t doesn't exist */
//	if (!t)
//		return 1;
//
//	/* NOTE: We cannot check for non-preemptibility since we
//	 *       don't know what address space we're currently in.
//	 */
//
//	/* make sure to get non-rt stuff out of the way */
//	if (!is_realtime(t))
//		return 1;
//
//	next = __next_ready(rt);
//	if (fp_higher_prio(next, t))
//		high_priority = 1;
//
//	/* Check if we have enough idle/can-preempt cache partitions */
//	used_cache_partitions =
//		count_set_bits(rt->used_cache_partitions & CACHE_PARTITIONS_MASK);
//	if (MAX_NUM_CACHE_PARTITIONS - used_cache_partitions
//		<= tsk_rt(t)->task_params.num_cache_partitions)
//	{
//		enough_cache = 1;
//	} else
//	{
//		/* TODO: check how many cache partitions can be preempted */
//		int cpu;
//		int num_preempt_cp = 0;
//		for (cpu = 0; cpu < NR_CPUS; cpu++)  {
//			cpu_entry_t* entry = gsnfpca_cpus[cpu];
//			struct task_struct* cur = entry->scheduled;
//
//			if (!is_realtime(cur))
//				continue;
//
//			if (fp_higher_prio(next, cur))
//				num_preempt_cp += tsk_rt(cur)->task_params.num_cache_partitions;
//		}
//		if (MAX_NUM_CACHE_PARTITIONS - used_cache_partitions + num_preempt_cp
//			<= tsk_rt(t)->task_params.num_cache_partitions)
//			enough_cache = 1;
//		else
//			enough_cache = 0;
//	}
//
//	if (high_priority && enough_cache)
//		return 1;
//}
void fp_prio_queue_init(struct fp_prio_queue* q)
{
	int i;

	for (i = 0; i < FP_PRIO_BIT_WORDS; i++)
		q->bitmask[i] = 0;
	for (i = 0; i < LITMUS_MAX_PRIORITY; i++)
		bheap_init(&q->queue[i]);
}

void fp_ready_list_init(struct fp_ready_list* q)
{
	int i;

	for (i = 0; i < FP_PRIO_BIT_WORDS; i++)
		q->bitmask[i] = 0;
	for (i = 0; i < LITMUS_MAX_PRIORITY; i++)
		INIT_LIST_HEAD(q->queue + i);
}

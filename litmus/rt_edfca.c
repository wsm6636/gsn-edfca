#include <linux/spinlock.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <litmus/litmus.h>
#include <litmus/jobs.h>
#include <litmus/sched_plugin.h>
#include <litmus/edf_common.h>
#include <litmus/sched_trace.h>
#include <litmus/trace.h>
#include <litmus/debug_trace.h>

#include <litmus/preempt.h>
#include <litmus/budget.h>

#include <litmus/rt_edfca.h>

/* CPU cache status for all cache-aware scheduler */
DEFINE_PER_CPU(cpu_edfca_entry_t, cpu_edfca_entries);

/* input
 * cpu: the cpu to check
 * cp_mask: the cp_mask the cpu has
 */
/*
static inline int check_edfca_status_invariant(int cpu, uint32_t cp_mask)
{
	int i;
	cpu_edfca_entry_t *edfca_entry_tmp, *edfca_entry;
	
	edfca_entry = &per_cpu(cpu_edfca_entries, cpu);

	for_each_online_cpu(i) {
		edfca_entry_tmp = &per_cpu(cpu_edfca_entries, i);
		if (i != cpu && (edfca_entry_tmp->used_cp & cp_mask))
		{
			TRACE("[BUG]Lock [P%d], Detect overlap  [P%d] used_cp:0x%x, [P%d] used_cp:0x%x, NR_CPUS=%d\n",
				   cpu, i, edfca_entry_tmp->used_cp, cpu, edfca_entry->used_cp, NR_CPUS);
			return 0;
		}
	}

	return 1;
}
*/
void edfca_flush_cache_partitions(int cpu, uint16_t cp_mask)
{
	if(cp_mask != 0){
		l2x0_flush_cache_ways(cp_mask);
		TRACE("edfca flush cache partitions 0x%x on cpu %d\n", cp_mask,cpu);
	}else
			TRACE("[BUG] lock cache partition 0 on cpu %d\n", cpu);
}
/* lock_edfca_partitions
 * lock cp_mask for cpu so that only cpu can use cp_mask
 * NOTE:
 * 1) rt.lock is grabbed by the caller so that
 *    scheduler on different CPUs do not have race condition
 * 2) We have race condition when user write to /proc/sys
 *    As long as users do not write to /proc/sys, we are safe
 *
 * tsk: lock bandwidth partition for task tsk
 */
void lock_edfca_partitions(int cpu, uint32_t cp_mask, struct task_struct *tsk, rt_domain_t *rt)
{
	cpu_edfca_entry_t *edfca_entry;
    int ret = 0, i;
	uint16_t cp_mask_to_flush = 0;

	if (cpu == NO_CPU)
	{
		TRACE("[BUG] try to lock 0x%x on NO_CPU\n", cp_mask);
	} else
	{
		edfca_entry = &per_cpu(cpu_edfca_entries, cpu);
		if (edfca_entry->used_cp != 0)
		{
			TRACE("[BUG][P%d] has locked bw 0x%x before try to lock cp 0x%x\n",
				  edfca_entry->cpu, edfca_entry->used_cp, cp_mask);
		}

			edfca_entry->used_cp = cp_mask;
			//cp_mask_to_flush = cp_mask;
			for (i = 0; i < MAX_CACHE_PARTITIONS; i++)
			{
				if (edfca_entry->used_cp & (1<<i) & MAX_CACHE_PARTITIONS)
				{
					//if (rt->l2_cps[i] != tsk->pid)
					//{
						cp_mask_to_flush |= (1 << i);
						rt->l2_cps[i] = tsk->pid;
					//}
				
				}
			}
			rt->used_cache_partitions |= cp_mask;
		//ret=__lock_cache_ways_to_cpu(cpu,cp_mask_to_flush);
		//ret=lock_cache_ways_to_cpu(cpu,cp_mask_to_flush);
		if (ret)
			{
 				TRACE("[BUG][P%d] PL310 lock cache 0x%d fails\n",
 					cpu, cp_mask_to_flush);
 			}
			//edfca_flush_cache_partitions(cpu,cp_mask_to_flush);
		
	}
	return;
}

/* unlock_edfca_partitions
 * unlock cp_mask for cpu so that other cpus can use cp_mask
 */
void unlock_edfca_partitions(int cpu, uint32_t cp_mask, rt_domain_t *rt)
{
	cpu_edfca_entry_t *edfca_entry;
    int ret, i;

	if (cpu == NO_CPU) 	
    {
		TRACE("[BUG] try to unlock 0x%x on NO_CPU\n", cp_mask);
	} else 	
    {
		edfca_entry = &per_cpu(cpu_edfca_entries, cpu);
		if (edfca_entry->used_cp != cp_mask)
		{
			TRACE("[BUG][P%d] has locked cache partitions 0x%x before try to unlock cache partitions 0x%x\n",
				  edfca_entry->cpu, edfca_entry->used_cp, cp_mask);
		}
	//	ret = check_edfca_status_invariant(cpu, cp_mask);
		for (i = 0; i < MAX_CACHE_PARTITIONS; i++)
		{
			if (edfca_entry->used_cp & (1<<i) & MAX_CACHE_PARTITIONS)
			{
				rt->l2_cps[i] = 0;
			}
		}
		edfca_entry->used_cp = 0;
		rt->used_cache_partitions &= (CACHE_PARTITIONS_MASK & ~cp_mask);
		//__unlock_cache_ways_to_cpu(cpu);
		//unlock_cache_ways_to_cpu(cpu);
	}

	return;
}

int edfca_count_set_bits(uint32_t bitmask)
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


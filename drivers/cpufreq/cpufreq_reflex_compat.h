/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Reflex CPUFreq governor: the only kernel-version differences.
 * cpufreq_reflex.c is byte-identical across versions; this file is not.
 *
 * Linux 4.9 backport notes:
 *
 *  - cpufreq_driver_adjust_perf() takes (unsigned int, unsigned long,
 *    unsigned long, unsigned long) here, as on 6.12, but the capacity
 *    argument is reached through the driver callback added to
 *    struct cpufreq_driver.
 *  - struct cpufreq_governor has no .flags member, so
 *    CPUFREQ_GOV_DYNAMIC_SWITCHING cannot be set.
 *  - arch_scale_cpu_capacity() takes (sd, cpu), not (cpu).
 *  - There is no arch_freq_scale: arch_scale_freq_capacity() is the
 *    static per-CPU capacity, so the frequency-scaled capacity has to
 *    be derived from the policy's current frequency.
 *  - cpufreq_policy_transition_delay_us() does not exist.
 *  - policy->dvfs_possible_from_any_cpu does not exist.
 *  - sched_setattr_nocheck() does not exist, and SCHED_DEADLINE
 *    governor kthreads are not used on this tree; the kthread is set
 *    up the way the 4.9 schedutil governor does it.
 */
#ifndef _CPUFREQ_REFLEX_COMPAT_H
#define _CPUFREQ_REFLEX_COMPAT_H

/*
 * 6.12 has to_gov_attr_set() in <linux/cpufreq.h>.  4.9 keeps it private
 * to drivers/cpufreq/cpufreq_governor_attr_set.c, so provide it here for
 * the governor's own tunables release callback.
 */
static inline struct gov_attr_set *to_gov_attr_set(struct kobject *kobj)
{
	return container_of(kobj, struct gov_attr_set, kobj);
}

/*
 * 6.12: cpufreq_driver_adjust_perf(policy->cpu, min, target, capacity).
 */
static inline void rfx_adjust_perf(struct cpufreq_policy *policy,
				   unsigned long min_perf,
				   unsigned long target_perf,
				   unsigned long max_perf,
				   unsigned long capacity)
{
	cpufreq_driver_adjust_perf(policy->cpu, min_perf, target_perf,
				   capacity);
}

/*
 * 6.12: arch_scale_cpu_capacity(cpu).  4.9: arch_scale_cpu_capacity(sd, cpu).
 */
static inline unsigned long rfx_arch_scale_cpu_capacity(int cpu)
{
	return arch_scale_cpu_capacity(NULL, cpu);
}

/*
 * 6.12: arch_scale_freq_capacity(cpu) returns arch_freq_scale, i.e. the
 * capacity of the CPU at its current frequency.  4.9 arm64 has no
 * arch_freq_scale, so derive it from the policy's current frequency the
 * same way topology_set_freq_scale() would.
 */
static inline unsigned long rfx_arch_scale_freq_capacity(
		struct cpufreq_policy *policy)
{
	if (!policy->cpuinfo.max_freq || !policy->cur)
		return SCHED_CAPACITY_SCALE;

	return div64_u64((u64)policy->cur * SCHED_CAPACITY_SCALE,
			 policy->cpuinfo.max_freq);
}

/*
 * 6.12: cpufreq_policy_transition_delay_us().  4.9: reproduce the core's
 * own derivation from the driver-reported transition latency.
 */
static inline unsigned int rfx_transition_delay_us(
		struct cpufreq_policy *policy)
{
	unsigned int latency = policy->cpuinfo.transition_latency / NSEC_PER_USEC;

	return latency ? latency : LATENCY_MULTIPLIER;
}

/*
 * 6.12 exposes arch_scale_freq_invariant() through <linux/sched/topology. h>.
 * 4.9 keeps it private to kernel/sched/sched.h, gated on whether the arch
 * defines arch_scale_freq_capacity.  Reproduce that same gate here so an
 * out-of-tree governor sees exactly what the in-tree schedutil sees.
 */
#ifdef arch_scale_freq_capacity
#define rfx_arch_scale_freq_invariant()	(true)
#else
#define rfx_arch_scale_freq_invariant()	(false)
#endif

/*
 * 6.12: sched_setattr_nocheck() with a SCHED_DEADLINE attr.  4.9 schedutil
 * sets its kthread to SCHED_FIFO at half of MAX_USER_RT_PRIO; do the same
 * so reflex never asks for a policy this kernel may not have enabled.
 */
static inline int rfx_kthread_set_sched(struct task_struct *thread)
{
	struct sched_param param = { .sched_priority = MAX_USER_RT_PRIO / 2 };

	return sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
}

#endif /* _CPUFREQ_REFLEX_COMPAT_H */

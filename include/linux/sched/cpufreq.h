/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_CPUFREQ_H
#define _LINUX_SCHED_CPUFREQ_H

#include <linux/types.h>

#ifdef CONFIG_CPU_FREQ

struct cpufreq_policy;

/*
 * Exported helpers for external cpufreq governors (e.g. loadable modules).
 * Implemented in kernel/sched/cpufreq_schedutil.c.
 */
unsigned long cpufreq_get_capacity_ref_freq(struct cpufreq_policy *policy);

void cpufreq_get_effective_util(int cpu, unsigned long boost,
				unsigned long *out_util,
				unsigned long *out_bw_min,
				unsigned long *out_bw_max);

bool cpufreq_cpu_dl_bw_exceeded(int cpu, unsigned long bw_min);

u64 cpufreq_pelt_decay(u64 val, u64 delta_ns);

bool cpufreq_this_cpu_can_update(struct cpufreq_policy *policy);

/*
 * 4.9 does not carry these two schedutil helpers in a header; keep them
 * here so external governors get the same mapping as 6.12.
 */
static inline unsigned long map_util_perf(unsigned long util)
{
	return util + (util >> 2);
}

static inline unsigned long map_util_freq(unsigned long util,
					  unsigned long freq,
					  unsigned long cap)
{
	return (freq + (freq >> 2)) * util / cap;
}

#endif /* CONFIG_CPU_FREQ */

#endif /* _LINUX_SCHED_CPUFREQ_H */

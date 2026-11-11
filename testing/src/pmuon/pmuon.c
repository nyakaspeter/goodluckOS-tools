// pmuon.ko: a test module (never shipped, unload after use) that switches the
// Cortex-A7's performance counters on for user space on every core. The stock
// goodluckOS kernel has no PMU driver, so perf can't count anything; with this
// loaded, testing/pmuread reads the cycle counter and four event counters of a
// core directly. insmod pmuon.ko events=0x08,0x01,0x03,0x10 (ARMv7 common
// event numbers: 0x08 instructions, 0x01/0x03 L1 I/D cache refill, 0x02/0x05
// I/D TLB refill, 0x10 branch mispredicted, 0x17 L2 data refill, 0x13 data
// memory access). rmmod turns user access off again.
#include <linux/init.h>
#include <linux/module.h>
#include <linux/smp.h>

static unsigned int events[4] = { 0x08, 0x01, 0x03, 0x10 };
module_param_array(events, uint, NULL, 0444);

static void pmuon_enable(void *unused)
{
	u32 v;
	int i;

	asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(v));
	v |= 1 | 2 | 4;   /* enable, reset event counters, reset cycle counter */
	v &= ~8u;         /* cycle counter counts every cycle */
	asm volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(v));
	for (i = 0; i < 4; i++) {
		asm volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(i));          /* PMSELR */
		asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(events[i])); /* PMXEVTYPER */
	}
	v = 0x8000000f;
	asm volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(v));  /* PMINTENCLR */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(v));  /* PMCNTENSET */
	v = 1;
	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(v));  /* PMUSERENR */
}

static void pmuon_disable(void *unused)
{
	u32 v = 0;

	asm volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(v));
	v = 0x8000000f;
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(v));  /* PMCNTENCLR */
}

static int __init pmuon_init(void)
{
	on_each_cpu(pmuon_enable, NULL, 1);
	pr_info("pmuon: counters on (events %#x %#x %#x %#x)\n", events[0], events[1], events[2], events[3]);
	return 0;
}

static void __exit pmuon_exit(void)
{
	on_each_cpu(pmuon_disable, NULL, 1);
}

module_init(pmuon_init);
module_exit(pmuon_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Performance counters for user space, for measurements");

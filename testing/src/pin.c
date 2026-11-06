// pin: set a thread's CPU affinity (busybox has no taskset).
//   pin <tid> <cpu mask, e.g. 0x2>
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	if(argc != 3){
		fprintf(stderr, "usage: pin <tid> <mask>\n");
		return 2;
	}
	unsigned long mask = strtoul(argv[2], NULL, 0);
	cpu_set_t set;
	CPU_ZERO(&set);
	for(int cpu = 0; cpu < 32; cpu++)
		if(mask & (1UL << cpu))
			CPU_SET(cpu, &set);
	if(sched_setaffinity(atoi(argv[1]), sizeof set, &set) != 0){
		perror("sched_setaffinity");
		return 1;
	}
	return 0;
}

// pmuread <cpu> <seconds> [name name name name]: with testing's pmuon.ko
// loaded, the cycle counter and the four event counters of one core over a
// time, per second and per 1000 of the first event (load pmuon with
// instructions as the first event). Pin the thread to measure to that core
// (testing/pin); everything that runs on the core is counted. The counters
// are 32 bits wide: keep the time short enough (2 s).
#define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void read_all(uint32_t *c) {
	uint32_t v;
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
	c[0] = v;
	for (uint32_t i = 0; i < 4; i++) {
		__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(i));
		__asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(v));
		c[i + 1] = v;
	}
}

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: pmuread <cpu> <seconds> [four event names]\n"); return 2; }
	int cpu = atoi(argv[1]), secs = atoi(argv[2]);
	cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set)) { perror("sched_setaffinity"); return 1; }
	uint32_t a[5], b[5];
	read_all(a);
	sleep(secs);
	read_all(b);
	const char *names[5] = { "cycles", "event 0", "event 1", "event 2", "event 3" };
	for (int i = 0; i < 4 && 3 + i < argc; i++) names[i + 1] = argv[3 + i];
	double first = (double)(uint32_t)(b[1] - a[1]) / secs;
	for (int i = 0; i < 5; i++) {
		double per = (double)(uint32_t)(b[i] - a[i]) / secs;
		printf("%-22s %13.0f /s", names[i], per);
		if (i >= 2 && first > 0) printf("  %8.2f per 1000 %s", per * 1000.0 / first, names[1]);
		printf("\n");
	}
	double cyc = (double)(uint32_t)(b[0] - a[0]) / secs;
	if (cyc > 0) printf("%s per cycle: %.2f\n", names[1], first / cyc);
	return 0;
}

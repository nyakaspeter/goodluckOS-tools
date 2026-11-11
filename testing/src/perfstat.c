// perfstat <tid> <seconds>: hardware counters of one thread (perf_event_open;
// root): cycles, instructions, cache and TLB misses, branch misses, each with
// its rate per 1000 instructions. Says whether a thread is short of
// instructions per second or stalled on memory. Counters the PMU or kernel
// does not have print as "n/a".
#define _GNU_SOURCE
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

struct ev { const char *name; uint32_t type; uint64_t config; int fd; };
#define HW(n, c) { n, PERF_TYPE_HARDWARE, c, -1 }
#define CACHE(n, id, op, res) { n, PERF_TYPE_HW_CACHE, (id) | ((op) << 8) | ((res) << 16), -1 }

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: perfstat <tid> <seconds>\n"); return 2; }
	int tid = atoi(argv[1]), secs = atoi(argv[2]);
	struct ev evs[] = {
		HW("cycles", PERF_COUNT_HW_CPU_CYCLES),
		HW("instructions", PERF_COUNT_HW_INSTRUCTIONS),
		HW("branch-misses", PERF_COUNT_HW_BRANCH_MISSES),
		HW("cache-misses (last level)", PERF_COUNT_HW_CACHE_MISSES),
		CACHE("L1 icache read misses", PERF_COUNT_HW_CACHE_L1I, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS),
		CACHE("L1 dcache read misses", PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS),
		CACHE("L1 dcache write misses", PERF_COUNT_HW_CACHE_L1D, PERF_COUNT_HW_CACHE_OP_WRITE, PERF_COUNT_HW_CACHE_RESULT_MISS),
		CACHE("iTLB misses", PERF_COUNT_HW_CACHE_ITLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS),
		CACHE("dTLB read misses", PERF_COUNT_HW_CACHE_DTLB, PERF_COUNT_HW_CACHE_OP_READ, PERF_COUNT_HW_CACHE_RESULT_MISS),
	};
	int n = sizeof(evs) / sizeof(evs[0]);
	// The PMU has four counters besides the cycle counter: measure in rounds.
	uint64_t val[16] = {0}; int ok[16] = {0};
	for (int round = 0; round < n; round += 3) {
		for (int i = round; i < n && i < round + 3; i++) {
			struct perf_event_attr a; memset(&a, 0, sizeof(a));
			a.size = sizeof(a); a.type = evs[i].type; a.config = evs[i].config;
			a.disabled = 1; a.exclude_kernel = 0; a.exclude_hv = 1;
			evs[i].fd = syscall(__NR_perf_event_open, &a, tid, -1, -1, 0);
			if (evs[i].fd >= 0) { ioctl(evs[i].fd, PERF_EVENT_IOC_RESET, 0); ioctl(evs[i].fd, PERF_EVENT_IOC_ENABLE, 0); }
		}
		sleep(secs);
		for (int i = round; i < n && i < round + 3; i++) {
			if (evs[i].fd < 0) continue;
			ioctl(evs[i].fd, PERF_EVENT_IOC_DISABLE, 0);
			if (read(evs[i].fd, &val[i], 8) == 8) ok[i] = 1;
			close(evs[i].fd);
		}
	}
	// Each round is its own time window: rates use the instruction count scaled per second.
	double instr = 0, cyc = 0;
	if (ok[1]) instr = (double)val[1] / secs;
	if (ok[0]) cyc = (double)val[0] / secs;
	for (int i = 0; i < n; i++) {
		if (!ok[i]) { printf("%-28s n/a\n", evs[i].name); continue; }
		double per = (double)val[i] / secs;
		printf("%-28s %14.0f /s", evs[i].name, per);
		if (i > 1 && instr > 0) printf("  %7.2f per 1000 instructions", per * 1000.0 / instr);
		printf("\n");
	}
	if (instr > 0 && cyc > 0) printf("instructions per cycle       %.2f\n", instr / cyc);
	return 0;
}

// tlbcal: what a TLB miss costs on this CPU and what the PMU's TLB refill
// events count. With pmuon.ko loaded as events=0x08,0x02,0x05,0x13, reads
// one byte of each of N pages in turn (a different cache line of each page,
// so the data stays in the L1 cache) for N below and above the TLB sizes, and
// prints cycles and data-TLB refills per access. Then the same for code: N
// pages each holding one `bx lr`, called in turn.
#define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

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

int main(void) {
	cpu_set_t set; CPU_ZERO(&set); CPU_SET(3, &set);
	sched_setaffinity(0, sizeof(set), &set);
	static const int sizes[] = { 8, 100, 200, 400, 800, 2000 };
	enum { MAXN = 2000, ROUNDS = 2000000 };
	volatile uint8_t *data = mmap(NULL, MAXN * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	uint32_t *code = mmap(NULL, MAXN * 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (data == MAP_FAILED || code == MAP_FAILED) { perror("mmap"); return 1; }
	for (int i = 0; i < MAXN; i++) {
		data[i * 4096 + (i * 64) % 4096] = 1;
		code[i * 1024 + (i * 16) % 1024] = 0xe12fff1e; /* bx lr */
	}
	__builtin___clear_cache((char *)code, (char *)code + MAXN * 4096);
	for (int kind = 0; kind < 2; kind++) {
		for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
			int n = sizes[s];
			uint32_t a[5], b[5], sum = 0;
			for (int warm = 0; warm < 2; warm++) {
				read_all(a);
				for (int r = 0, i = 0; r < ROUNDS; r++) {
					if (kind == 0) sum += data[i * 4096 + (i * 64) % 4096];
					else ((void (*)(void))(code + i * 1024 + (i * 16) % 1024))();
					if (++i == n) i = 0;
				}
				read_all(b);
			}
			printf("%s %4d pages: %6.1f cycles, %5.2f instructions, %5.3f itlb, %5.3f dtlb refills per access (%u)\n",
			       kind ? "code" : "data", n, (double)(uint32_t)(b[0] - a[0]) / ROUNDS, (double)(uint32_t)(b[1] - a[1]) / ROUNDS,
			       (double)(uint32_t)(b[2] - a[2]) / ROUNDS, (double)(uint32_t)(b[3] - a[3]) / ROUNDS, sum);
		}
	}
	return 0;
}

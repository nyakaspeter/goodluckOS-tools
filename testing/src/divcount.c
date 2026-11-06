// divcount.so: LD_PRELOAD counter of who calls the 64-bit division helpers
// (__aeabi_uldivmod, __aeabi_ldivmod): a histogram of return addresses,
// printed to stderr every 2 s as calls per second. LCSNative is not PIE, so
// the addresses are the ELF's (resolve with nm -n of the unstripped build).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static void *real_unsigned, *real_signed;
#define SLOTS 512
static struct { uintptr_t address; unsigned long count; } slots[SLOTS];
static unsigned long total;
static struct timespec last;

__attribute__((constructor)) static void
divcount_init(void)
{
	real_unsigned = dlsym(RTLD_NEXT, "__aeabi_uldivmod");
	real_signed = dlsym(RTLD_NEXT, "__aeabi_ldivmod");
}

// Not thread safe on purpose (counts only): the game thread is the caller of interest.
static void *
note(uintptr_t lr, void *real)
{
	total++;
	int i;
	for(i = 0; i < SLOTS; i++){
		if(slots[i].address == lr){ slots[i].count++; break; }
		if(slots[i].address == 0){ slots[i].address = lr; slots[i].count = 1; break; }
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double seconds = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
	if(seconds >= 2.0){
		if(last.tv_sec != 0){
			fprintf(stderr, "divcount/s: total %lu", (unsigned long)(total / seconds));
			for(int pass = 0; pass < 10; pass++){
				int best = -1;
				for(i = 0; i < SLOTS && slots[i].address; i++)
					if(slots[i].count && (best < 0 || slots[i].count > slots[best].count)) best = i;
				if(best < 0) break;
				fprintf(stderr, " %08lx:%lu", (unsigned long)slots[best].address, (unsigned long)(slots[best].count / seconds));
				slots[best].count = 0;
			}
			fprintf(stderr, "\n");
		}
		for(i = 0; i < SLOTS; i++) slots[i].address = slots[i].count = 0;
		total = 0;
		last = now;
	}
	return real;
}

void *divcount_note_unsigned(uintptr_t lr) { return note(lr, real_unsigned); }
void *divcount_note_signed(uintptr_t lr) { return note(lr, real_signed); }

// The helpers take the dividend in r0:r1 and the divisor in r2:r3 and return
// the quotient in r0:r1 and the remainder in r2:r3: keep all four and the
// link register, record the caller, then jump to the real helper.
__attribute__((naked)) void
__aeabi_uldivmod(void)
{
	__asm__ volatile(
		"push {r0-r4, lr}\n"
		"mov r0, lr\n"
		"bl divcount_note_unsigned\n"
		"mov r12, r0\n"
		"pop {r0-r4, lr}\n"
		"bx r12\n");
}

__attribute__((naked)) void
__aeabi_ldivmod(void)
{
	__asm__ volatile(
		"push {r0-r4, lr}\n"
		"mov r0, lr\n"
		"bl divcount_note_signed\n"
		"mov r12, r0\n"
		"pop {r0-r4, lr}\n"
		"bx r12\n");
}

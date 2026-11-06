// psample: where a thread spends its time, by sampling its program counter
// with ptrace (root). Prints the share per mapping (library, or [anon] for
// JIT code) and the hottest 64-byte blocks as <file>+<offset>, then who
// called into libc for the samples that landed there (the link register:
// memcpy, memset and syscall stubs don't change it), as "caller <file>+<offset>".
//   psample <tid> [secs] [hz] [blocks]   (blocks: how many hot blocks to list, default 60)
//   PSAMPLE_SO=<base>: count <base>..+16 MB as /libGTALcs.so (a loader's anonymous mapping)
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXS 100000

struct map { unsigned long lo, hi, off; char name[256]; };
static struct map maps[32768];
static int nmaps;
static unsigned long long pcs[MAXS];
static unsigned long long lrs[MAXS];
static unsigned long long rawpcs[MAXS], rawlrs[MAXS];

static int
cmpul(const void *a, const void *b)
{
	unsigned long long x = *(const unsigned long long*)a, y = *(const unsigned long long*)b;
	return x < y ? -1 : x > y;
}

static struct map *
find(unsigned long pc)
{
	for(int i = 0; i < nmaps; i++)
		if(pc >= maps[i].lo && pc < maps[i].hi)
			return &maps[i];
	return NULL;
}

int
main(int argc, char **argv)
{
	if(argc < 2){
		fprintf(stderr, "usage: psample <tid> [secs] [hz] [blocks]\n");
		return 2;
	}
	int tid = atoi(argv[1]);
	double secs = argc > 2 ? atof(argv[2]) : 10;
	int hz = argc > 3 ? atoi(argv[3]) : 200;
	int ntop = argc > 4 ? atoi(argv[4]) : 60;
	if(ntop < 1 || ntop > 2000)
		ntop = 60;
	int n = 0;
	struct timespec gap = { 0, 1000000000L / hz };
	for(long i = 0; i < secs * hz && n < MAXS; i++){
		if(ptrace(PTRACE_ATTACH, tid, 0, 0) == 0){
			int st;
			if(waitpid(tid, &st, __WALL) == tid){
				unsigned long regs[18];
				if(ptrace(PTRACE_GETREGS, tid, 0, regs) == 0){
					lrs[n] = regs[14] & ~1UL;	// lr (Thumb bit off)
					rawlrs[n] = lrs[n];
					rawpcs[n] = regs[15];
					pcs[n++] = regs[15];	// pc
				}
			}
			ptrace(PTRACE_DETACH, tid, 0, 0);
		}
		nanosleep(&gap, NULL);
	}
	char path[64], line[512];
	// PSAMPLE_SO=<base>: the game library a loader mapped anonymously, from
	// that address over 16 MB, counted as /libGTALcs.so (offsets = file offsets)
	if(getenv("PSAMPLE_SO")){
		maps[0].lo = strtoul(getenv("PSAMPLE_SO"), NULL, 0);
		maps[0].hi = maps[0].lo + (16 << 20);
		maps[0].off = 0;
		strcpy(maps[0].name, "/libGTALcs.so");
		nmaps = 1;
	}
	snprintf(path, sizeof path, "/proc/%d/maps", tid);
	FILE *f = fopen(path, "r");
	while(f && fgets(line, sizeof line, f) && nmaps < 32768){
		struct map *m = &maps[nmaps];
		m->name[0] = 0;
		if(sscanf(line, "%lx-%lx %*s %lx %*s %*s %255s", &m->lo, &m->hi, &m->off, m->name) >= 3){
			if(!m->name[0])
				strcpy(m->name, "[anon]");
			nmaps++;
		}
	}
	printf("%d samples\n", n);
	// Share per mapping name.
	static char names[4096][256];
	static int counts[4096];
	int nn = 0;
	for(int i = 0; i < n; i++){
		struct map *m = find(pcs[i]);
		const char *nm = m ? m->name : "?";
		int j;
		for(j = 0; j < nn && strcmp(names[j], nm); j++)
			;
		if(j == nn)
			strcpy(names[nn++], nm);
		counts[j]++;
	}
	for(int k = 0; k < nn; k++){
		int best = k;
		for(int j = k + 1; j < nn; j++)
			if(counts[j] > counts[best])
				best = j;
		char t[256]; int c = counts[k];
		strcpy(t, names[k]); strcpy(names[k], names[best]); strcpy(names[best], t);
		counts[k] = counts[best]; counts[best] = c;
		if(counts[k] * 100 >= n)
			printf("%5.1f%% %s\n", counts[k] * 100.0 / n, names[k]);
	}
	// Callers of libc, keyed like the blocks below (exact return address).
	int nlibc = 0;
	for(int i = 0; i < n; i++){
		struct map *m = find(pcs[i]);
		struct map *c = m && strstr(m->name, "/libc.") ? find(lrs[i]) : NULL;
		lrs[i] = c && c->name[0] == '/' ? (unsigned long long)(c - maps) << 32 | (lrs[i] - c->lo + c->off) : 0;
		nlibc += m && strstr(m->name, "/libc.") != NULL;
	}
	qsort(lrs, n, sizeof lrs[0], cmpul);
	static struct { unsigned long long key; int c; } callers[40];
	for(int i = 0; i < n;){
		int j = i;
		while(j < n && lrs[j] == lrs[i])
			j++;
		if(lrs[i])
			for(int k = 0; k < 40; k++)
				if(j - i > callers[k].c){
					memmove(&callers[k + 1], &callers[k], (39 - k) * sizeof callers[0]);
					callers[k].key = lrs[i];
					callers[k].c = j - i;
					break;
				}
		i = j;
	}
	// Hottest 64-byte blocks in files, keyed as (map index << 32 | file offset).
	for(int i = 0; i < n; i++){
		struct map *m = find(pcs[i]);
		if(m && m->name[0] == '/')
			pcs[i] = (unsigned long long)(m - maps) << 32 | ((pcs[i] - m->lo + m->off) & ~63UL);
		else
			pcs[i] = 0;
	}
	qsort(pcs, n, sizeof pcs[0], cmpul);
	static struct { unsigned long long key; int c; } top[2000];
	for(int i = 0; i < n;){
		int j = i;
		while(j < n && pcs[j] == pcs[i])
			j++;
		if(pcs[i])
			for(int k = 0; k < ntop; k++)
				if(j - i > top[k].c){
					memmove(&top[k + 1], &top[k], (ntop - 1 - k) * sizeof top[0]);
					top[k].key = pcs[i];
					top[k].c = j - i;
					break;
				}
		i = j;
	}
	for(int k = 0; k < ntop && top[k].c; k++){
		struct map *m = &maps[top[k].key >> 32];
		const char *b = strrchr(m->name, '/');
		printf("%5.1f%% %s+0x%lx\n", top[k].c * 100.0 / n, b ? b + 1 : m->name, (unsigned long)(top[k].key & 0xffffffff));
	}
	// PSAMPLE_PAIRS=1: the hottest (64-byte block, caller) pairs, to see who
	// calls a hot leaf function (the link register is its return address).
	if(getenv("PSAMPLE_PAIRS")){
		static struct { unsigned long long key, lr; int c; } pairs[60];
		for(int i = 0; i < n; i++){
			struct map *m = find(rawpcs[i]), *c = find(rawlrs[i]);
			if(!m || !c || m->name[0] != '/' || c->name[0] != '/')
				continue;
			unsigned long long key = (unsigned long long)(m - maps) << 32 | ((rawpcs[i] - m->lo + m->off) & ~63UL);
			unsigned long long lr = (unsigned long long)(c - maps) << 32 | (rawlrs[i] - c->lo + c->off);
			int k;
			for(k = 0; k < 60 && pairs[k].c && !(pairs[k].key == key && pairs[k].lr == lr); k++)
				;
			if(k < 60){
				pairs[k].key = key;
				pairs[k].lr = lr;
				pairs[k].c++;
			}
		}
		for(int a = 0; a < 60; a++)
			for(int b = a + 1; b < 60; b++)
				if(pairs[b].c > pairs[a].c){
					typeof(pairs[0]) t = pairs[a]; pairs[a] = pairs[b]; pairs[b] = t;
				}
		for(int k = 0; k < 60 && pairs[k].c; k++){
			struct map *m = &maps[pairs[k].key >> 32], *c = &maps[pairs[k].lr >> 32];
			const char *b = strrchr(m->name, '/'), *bc = strrchr(c->name, '/');
			printf("%5.1f%% pair %s+0x%lx from %s+0x%lx\n", pairs[k].c * 100.0 / n, b ? b + 1 : m->name,
			       (unsigned long)(pairs[k].key & 0xffffffff), bc ? bc + 1 : c->name, (unsigned long)(pairs[k].lr & 0xffffffff));
		}
	}
	if(nlibc)
		printf("libc callers (%.1f%% of samples in libc)\n", nlibc * 100.0 / n);
	for(int k = 0; k < 40 && callers[k].c; k++){
		struct map *m = &maps[callers[k].key >> 32];
		const char *b = strrchr(m->name, '/');
		printf("%5.1f%% caller %s+0x%lx\n", callers[k].c * 100.0 / n, b ? b + 1 : m->name,
		       (unsigned long)(callers[k].key & 0xffffffff));
	}
	return 0;
}

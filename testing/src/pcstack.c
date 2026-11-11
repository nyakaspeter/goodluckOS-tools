// pcstack: who is on the stack when a thread is inside one function. Samples
// the thread with ptrace (root); for samples whose PC lies in <lib>+<lo>..<hi>
// it scans the stack for return addresses into the named libraries and counts
// the chains of the first four found (innermost first), as <lib>+<offset>.
// For a hot libc routine whose caller a PC sampler can't tell (memcmp uses lr
// as a scratch register).
//   pcstack <tid> <lib substring> <lo hex> <hi hex> <secs> <hz> <scan lib substring> [<scan lib substring>]
//   e.g. pcstack 1234 /libc. 72ccc 72fac 20 400 libMali PPSSPPSDL
#define _FILE_OFFSET_BITS 64  // the stack is above 2 GB
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct map { unsigned long lo, hi, off; char name[256]; int exec; };
static struct map maps[4096];
static int nmaps;

static struct map *
find(unsigned long a)
{
	for(int i = 0; i < nmaps; i++)
		if(a >= maps[i].lo && a < maps[i].hi)
			return &maps[i];
	return NULL;
}

int
main(int argc, char **argv)
{
	if(argc < 8){
		fprintf(stderr, "usage: pcstack <tid> <lib> <lo> <hi> <secs> <hz> <scan lib> [<scan lib>]\n");
		return 2;
	}
	int tid = atoi(argv[1]);
	unsigned long lo = strtoul(argv[3], NULL, 16), hi = strtoul(argv[4], NULL, 16);
	double secs = atof(argv[5]);
	int hz = atoi(argv[6]);
	char path[64], line[512];
	snprintf(path, sizeof path, "/proc/%d/maps", tid);
	FILE *f = fopen(path, "r");
	char perms[8];
	while(f && fgets(line, sizeof line, f) && nmaps < 4096){
		struct map *m = &maps[nmaps];
		m->name[0] = 0;
		if(sscanf(line, "%lx-%lx %7s %lx %*s %*s %255s", &m->lo, &m->hi, perms, &m->off, m->name) >= 4){
			m->exec = perms[2] == 'x';
			nmaps++;
		}
	}
	snprintf(path, sizeof path, "/proc/%d/mem", tid);
	int mem = open(path, O_RDONLY);
	static struct { char chain[160]; int c; } tab[2048];
	int ntab = 0, total = 0, hits = 0;
	struct timespec gap = { 0, 1000000000L / hz };
	for(long i = 0; i < secs * hz; i++){
		if(ptrace(PTRACE_ATTACH, tid, 0, 0) == 0){
			int st;
			if(waitpid(tid, &st, __WALL) == tid){
				unsigned long regs[18];
				if(ptrace(PTRACE_GETREGS, tid, 0, regs) == 0){
					total++;
					struct map *m = find(regs[15]);
					unsigned long off = m ? regs[15] - m->lo + m->off : 0;
					if(m && strstr(m->name, argv[2]) && off >= lo && off < hi){
						hits++;
						uint32_t stack[768];
						ssize_t got = pread(mem, stack, sizeof stack, regs[13]);
						char chain[160] = "";
						int found = 0;
						for(int w = 0; w < got / 4 && found < 4; w++){
							struct map *c = find(stack[w] & ~1UL);
							if(!c || !c->exec)
								continue;
							int want = 0;
							for(int a = 7; a < argc; a++)
								want |= strstr(c->name, argv[a]) != NULL;
							if(!want)
								continue;
							const char *base = strrchr(c->name, '/');
							char one[64];
							snprintf(one, sizeof one, "%s%s+%lx", found ? " < " : "", base ? base + 1 : c->name, ((stack[w] & ~1UL) - c->lo + c->off) & ~0xfUL);
							strncat(chain, one, sizeof chain - strlen(chain) - 1);
							found++;
						}
						int k;
						for(k = 0; k < ntab && strcmp(tab[k].chain, chain); k++)
							;
						if(k == ntab && ntab < 2048)
							strcpy(tab[ntab++].chain, chain);
						if(k < 2048)
							tab[k].c++;
					}
				}
			}
			ptrace(PTRACE_DETACH, tid, 0, 0);
		}
		nanosleep(&gap, NULL);
	}
	printf("%d samples, %d in range (%.1f%%)\n", total, hits, total ? hits * 100.0 / total : 0.0);
	for(int n = 0; n < 25; n++){
		int best = -1;
		for(int k = 0; k < ntab; k++)
			if(tab[k].c && (best < 0 || tab[k].c > tab[best].c))
				best = k;
		if(best < 0)
			break;
		printf("%5.1f%% %s\n", tab[best].c * 100.0 / hits, tab[best].chain);
		tab[best].c = 0;
	}
	return 0;
}

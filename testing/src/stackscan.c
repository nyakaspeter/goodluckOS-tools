// stackscan: where each thread of a process is waiting, as the return
// addresses into one code range found on its stack (ptrace, root). For a
// loader-mapped game library, which /proc/<pid>/maps lists as anonymous.
//   stackscan <pid> <code start hex> <code end hex> [words, default 2048]
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: stackscan <pid> <start> <end> [words]\n");
		return 1;
	}
	int pid = atoi(argv[1]);
	uintptr_t lo = strtoul(argv[2], NULL, 16), hi = strtoul(argv[3], NULL, 16);
	int words = argc > 4 ? atoi(argv[4]) : 2048;
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/task", pid);
	DIR *d = opendir(path);
	if (!d)
		return 1;
	snprintf(path, sizeof(path), "/proc/%d/mem", pid);
	int mem = open(path, O_RDONLY);
	uint32_t *buf = malloc(words * 4);
	struct dirent *e;
	while ((e = readdir(d))) {
		int tid = atoi(e->d_name);
		if (tid <= 0)
			continue;
		if (ptrace(PTRACE_ATTACH, tid, 0, 0) < 0)
			continue;
		waitpid(tid, NULL, __WALL);
		struct user_regs regs;
		ptrace(PTRACE_GETREGS, tid, 0, &regs);
		uint32_t sp = regs.uregs[13], pc = regs.uregs[15], lr = regs.uregs[14];
		ssize_t n = 0;
		for (int i = 0; i < words; i++, n += 4) {
			errno = 0;
			long w = ptrace(PTRACE_PEEKDATA, tid, sp + i * 4, 0);
			if (errno)
				break;
			buf[i] = w;
		}
		ptrace(PTRACE_DETACH, tid, 0, 0);
		printf("== %d pc %08x lr %08x (%d stack words)\n", tid, pc, lr, (int)n / 4);
		int shown = 0;
		for (int i = 0; i < n / 4 && shown < 12; i++)
			if (buf[i] >= lo && buf[i] < hi) {
				printf("  +%x\n", buf[i] - (uint32_t)lo);
				shown++;
			}
	}
	return 0;
}

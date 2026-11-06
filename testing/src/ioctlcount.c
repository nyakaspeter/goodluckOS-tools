// ioctlcount.so: LD_PRELOAD counter of ioctl() calls by request code, printed
// to stderr every 2 s (calls per second), to see what a GL driver asks of its
// kernel module per frame, and how long each kind takes (ms per second: time
// a thread spends in it, waits for the GPU included).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define SLOTS 64
static struct { unsigned long request; unsigned long count; unsigned long ns; } counts[SLOTS];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct timespec last;
// MALI_IOC_MEM_WRITE_SAFE: sizes and time spent (struct: ctx, src, dest, size).
#define WRITE_SAFE 0xc020830aUL
static unsigned long ws_calls, ws_bytes, ws_small, ws_ns;

int
ioctl(int fd, unsigned long request, ...)
{
	static int (*real)(int, unsigned long, void *);
	if(!real)
		real = (int (*)(int, unsigned long, void *))dlsym(RTLD_NEXT, "ioctl");
	va_list ap;
	va_start(ap, request);
	void *arg = va_arg(ap, void *);
	va_end(ap);
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	int ret = real(fd, request, arg);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	unsigned long ns = (t1.tv_sec - t0.tv_sec) * 1000000000UL + (t1.tv_nsec - t0.tv_nsec);
	pthread_mutex_lock(&lock);
	if(request == WRITE_SAFE){
		unsigned int size = *(unsigned int *)((char *)arg + 24);
		ws_calls++;
		ws_bytes += size;
		ws_small += size <= 512;
		ws_ns += ns;
	}
	for(int i = 0; i < SLOTS; i++)
		if(counts[i].request == request || counts[i].count == 0){
			counts[i].request = request;
			counts[i].count++;
			counts[i].ns += ns;
			break;
		}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double seconds = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
	if(seconds >= 2.0){
		if(last.tv_sec != 0){
			fprintf(stderr, "ioctl/s:");
			for(int i = 0; i < SLOTS && counts[i].count; i++)
				fprintf(stderr, " %08lx:%.0f/%.0fms", counts[i].request, counts[i].count / seconds,
				        counts[i].ns / 1e6 / seconds);
			fprintf(stderr, "\n");
			if(ws_calls)
				fprintf(stderr, "write_safe: %lu calls, avg %lu bytes, %lu%% <=512, avg %lu us/call\n",
				        ws_calls, ws_bytes / ws_calls, ws_small * 100 / ws_calls, ws_ns / ws_calls / 1000);
		}
		ws_calls = ws_bytes = ws_small = ws_ns = 0;
		for(int i = 0; i < SLOTS; i++)
			counts[i].count = counts[i].ns = 0;
		last = now;
	}
	pthread_mutex_unlock(&lock);
	return ret;
}

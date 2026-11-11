// memcmpcount.so: LD_PRELOAD that counts memcmp (and bcmp, __memcmpeq) calls by caller and size.
// Prints the top callers to stderr every 100 MB asked for: calls, bytes asked
// for, bytes compared before the first difference, and the caller as <mapping>+<offset> (resolve with /proc/<pid>/maps
// printed once). For finding out what a GL driver compares on every draw.
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define N 4096
static struct { uintptr_t ra; unsigned long calls, bytes, compared; } tab[N];
static unsigned long total, totalBytes, nextDump = 100000000;

static void dump(void)
{
	static int maps;
	if (!maps) {
		maps = 1;
		FILE *f = fopen("/proc/self/maps", "r");
		char line[512];
		while (f && fgets(line, sizeof line, f))
			if (strstr(line, "r-xp") && (strstr(line, "libMali") || strstr(line, "PPSSPPSDL") || strstr(line, "libc.so")))
				fprintf(stderr, "MEMCMP map %s", line);
		if (f) fclose(f);
	}
	for (int k = 0; k < 8; k++) {
		int best = -1;
		for (int i = 0; i < N; i++)
			if (tab[i].calls && (best < 0 || tab[i].bytes > tab[best].bytes)) best = i;
		if (best < 0) break;
		fprintf(stderr, "MEMCMP caller %lx calls %lu bytes %lu avg %lu compared avg %lu\n", (unsigned long)tab[best].ra, tab[best].calls, tab[best].bytes, tab[best].bytes / tab[best].calls, tab[best].compared / tab[best].calls);
		tab[best].calls = tab[best].bytes = 0;
	}
	fprintf(stderr, "MEMCMP ---\n");
	memset(tab, 0, sizeof tab);
}

static int counted(const void *a, const void *b, size_t n, uintptr_t ra)
{
	unsigned h = (ra >> 2) % N;
	int slot = -1;
	for (int i = 0; i < 8; i++, h = (h + 1) % N) {
		if (tab[h].ra == ra || !tab[h].calls) { tab[h].ra = ra; tab[h].calls++; tab[h].bytes += n; slot = h; break; }
	}
	// MEMCMPCOUNT_SITE=<hex, low 12 bits of a caller>: sizes asked for from that caller.
	static long site = -2;
	static struct { size_t n; unsigned long calls, compared; } sizes[64];
	if (site == -2) site = getenv("MEMCMPCOUNT_SITE") ? strtol(getenv("MEMCMPCOUNT_SITE"), NULL, 16) : -1;
	int sizeSlot = -1;
	if (site >= 0 && (long)(ra & 0xfff) == site) {
		for (int i = 0; i < 64; i++)
			if (sizes[i].n == n || !sizes[i].calls) { sizes[i].n = n; sizes[i].calls++; sizeSlot = i; break; }
	}
	total++; totalBytes += n;
	if (totalBytes >= nextDump && site >= 0) {
		for (int i = 0; i < 64 && sizes[i].calls; i++)
			fprintf(stderr, "MEMCMP size %lu calls %lu compared avg %lu\n", (unsigned long)sizes[i].n, sizes[i].calls, sizes[i].compared / sizes[i].calls);
		memset(sizes, 0, sizeof sizes);
		sizeSlot = -1;
	}
	if (totalBytes >= nextDump) { nextDump = totalBytes + 100000000; fprintf(stderr, "MEMCMP total calls %lu bytes %lu\n", total, totalBytes); dump(); }
	const unsigned char *p = a, *q = b;
	int r = 0;
	// Word at a time while equal (the driver's blocks are mostly equal).
	while (n >= 4 && *(const uint32_t *)p == *(const uint32_t *)q) { p += 4; q += 4; n -= 4; }
	while (n--) { if (*p != *q) { r = *p - *q; break; } p++; q++; }
	if (slot >= 0) tab[slot].compared += p - (const unsigned char *)a;
	if (sizeSlot >= 0) sizes[sizeSlot].compared += p - (const unsigned char *)a;
	return r;
}

// The same routine under its three names (compilers call __memcmpeq when only equality matters).
int memcmp(const void *a, const void *b, size_t n) { return counted(a, b, n, (uintptr_t)__builtin_return_address(0)); }
int bcmp(const void *a, const void *b, size_t n) { return counted(a, b, n, (uintptr_t)__builtin_return_address(0)); }
int __memcmpeq(const void *a, const void *b, size_t n) { return counted(a, b, n, (uintptr_t)__builtin_return_address(0)); }

// libc functions that compare memory inside libc (their memcmp calls don't
// come through here): calls and the size they were asked to work on.
#include <dlfcn.h>
#include <search.h>
static void note(const char *what, size_t n, uintptr_t ra)
{
	static struct { const char *what; uintptr_t ra; unsigned long calls, bytes; } t[64];
	static unsigned long total;
	for (int i = 0; i < 64; i++) {
		if (!t[i].what || (t[i].what == what && t[i].ra == ra)) { t[i].what = what; t[i].ra = ra; t[i].calls++; t[i].bytes += n; break; }
	}
	if (++total % 200000 == 0) {
		for (int i = 0; i < 64 && t[i].what; i++)
			if (t[i].calls > 2000)
				fprintf(stderr, "MEMCMP via %s caller %lx calls %lu bytes %lu\n", t[i].what, (unsigned long)t[i].ra, t[i].calls, t[i].bytes);
		fprintf(stderr, "MEMCMP via ---\n");
		memset(t, 0, sizeof t);
	}
}
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	static void *(*real)(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
	if (!real) real = dlsym(RTLD_NEXT, "bsearch");
	note("bsearch", nmemb * size, (uintptr_t)__builtin_return_address(0));
	return real(key, base, nmemb, size, compar);
}
void *memmem(const void *h, size_t hl, const void *n, size_t nl)
{
	static void *(*real)(const void *, size_t, const void *, size_t);
	if (!real) real = dlsym(RTLD_NEXT, "memmem");
	note("memmem", hl, (uintptr_t)__builtin_return_address(0));
	return real(h, hl, n, nl);
}
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	static void (*real)(void *, size_t, size_t, int (*)(const void *, const void *));
	if (!real) real = dlsym(RTLD_NEXT, "qsort");
	note("qsort", nmemb * size, (uintptr_t)__builtin_return_address(0));
	real(base, nmemb, size, compar);
}
char *strstr(const char *h, const char *n)
{
	static char *(*real)(const char *, const char *);
	if (!real) real = dlsym(RTLD_NEXT, "strstr");
	note("strstr", strlen(h), (uintptr_t)__builtin_return_address(0));
	return real(h, n);
}
char *getenv(const char *name)
{
	static char *(*real)(const char *);
	if (!real) real = dlsym(RTLD_NEXT, "getenv");
	note("getenv", 0, (uintptr_t)__builtin_return_address(0));
	return real(name);
}

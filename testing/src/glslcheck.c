// glslcheck <file.vert|file.frag> ...: compile each file with the GLES 2
// driver that is active (run it through a port's glos-gpu for ARM's Mali
// driver) and print the compiler's verdict and log. Two files given as
// "a.vert+b.frag" are also linked. For finding out what a driver's shader
// compiler accepts without rebuilding a game.
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*fn)();
static void *egl, *gl;
#define E(n) ((fn)dlsym(egl, n))
#define G(n) ((fn)dlsym(gl, n))

static char *slurp(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) { perror(path); return 0; }
	static char buf[1 << 18];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	buf[n] = 0;
	fclose(f);
	return strdup(buf);
}

static unsigned compile(const char *path) {
	char *src = slurp(path);
	if (!src) return 0;
	int frag = strstr(path, ".frag") != 0;
	unsigned s = (unsigned)(long)G("glCreateShader")(frag ? 0x8B30 : 0x8B31);
	G("glShaderSource")(s, 1, &src, 0);
	G("glCompileShader")(s);
	int ok = 0; char log[4096] = "";
	G("glGetShaderiv")(s, 0x8B81, &ok);
	G("glGetShaderInfoLog")(s, (int)sizeof(log), 0, log);
	char *nl = strchr(log, '\n'); if (nl) *nl = 0;
	printf("%-28s %s %s\n", path, ok ? "ok    " : "FAILED", log);
	free(src);
	return ok ? s : 0;
}

int main(int argc, char **argv) {
	egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_GLOBAL);
	gl = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_GLOBAL);
	if (!egl || !gl) { fprintf(stderr, "%s\n", dlerror()); return 1; }
	void *dpy = E("eglGetDisplay")(0);
	if (!E("eglInitialize")(dpy, 0, 0)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
	int cattr[] = { 0x3040, 4, 0x3033, 1, 0x3038 };  // RENDERABLE_TYPE ES2, SURFACE_TYPE PBUFFER
	void *cfg; int n = 0;
	E("eglChooseConfig")(dpy, cattr, &cfg, 1, &n);
	if (!n) { fprintf(stderr, "no config\n"); return 1; }
	int sattr[] = { 0x3057, 16, 0x3056, 16, 0x3038 };
	void *surf = E("eglCreatePbufferSurface")(dpy, cfg, sattr);
	int xattr[] = { 0x3098, 2, 0x3038 };
	void *ctx = E("eglCreateContext")(dpy, cfg, 0, xattr);
	if (!E("eglMakeCurrent")(dpy, surf, surf, ctx)) { fprintf(stderr, "eglMakeCurrent failed (0x%x)\n", (int)(long)E("eglGetError")()); return 1; }
	fprintf(stderr, "%s\n", (const char *)G("glGetString")(0x1F01));
	for (int i = 1; i < argc; i++) {
		char *plus = strchr(argv[i], '+');
		if (!plus) { compile(argv[i]); continue; }
		*plus = 0;
		unsigned a = compile(argv[i]), b = compile(plus + 1);
		if (!a || !b) continue;
		unsigned p = (unsigned)(long)G("glCreateProgram")();
		G("glAttachShader")(p, a); G("glAttachShader")(p, b);
		G("glLinkProgram")(p);
		int ok = 0; char log[4096] = "";
		G("glGetProgramiv")(p, 0x8B82, &ok);
		G("glGetProgramInfoLog")(p, (int)sizeof(log), 0, log);
		printf("%-28s %s %s\n", "  link", ok ? "ok    " : "FAILED", log);
	}
	printf("DONE\n"); fflush(stdout);
	return 0;
}

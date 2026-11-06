// fbshot: the visible page of /dev/fb0 as a binary PPM on stdout, scaled down
// by an integer factor (default 2). For seeing the screen over the serial
// console while an app draws to the framebuffer (ARM's driver; 32 bpp).
//   fbshot [factor] | gzip | base64
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	int f = argc > 1 ? atoi(argv[1]) : 2;
	int fd = open("/dev/fb0", O_RDONLY);
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo fix;
	if(fd < 0 || f < 1 || ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 ||
	   ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0 || v.bits_per_pixel != 32){
		fprintf(stderr, "fbshot: no 32 bpp /dev/fb0\n");
		return 1;
	}
	unsigned char *fb = mmap(NULL, fix.smem_len, PROT_READ, MAP_SHARED, fd, 0);
	if(fb == MAP_FAILED){
		perror("fbshot: mmap");
		return 1;
	}
	int w = v.xres / f, h = v.yres / f;
	printf("P6\n%d %d\n255\n", w, h);
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++){
			unsigned char *p = fb + (size_t)(v.yoffset + y * f) * fix.line_length + (size_t)x * f * 4;
			putchar(p[v.red.offset / 8]);
			putchar(p[v.green.offset / 8]);
			putchar(p[v.blue.offset / 8]);
		}
	return 0;
}

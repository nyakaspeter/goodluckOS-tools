// fbprobe: average colour of a rectangle of the visible framebuffer page
// (ARM's driver draws to /dev/fb0), to tell game states apart from a shell.
//   fbprobe <x> <y> <w> <h>    prints "r g b" (0-255)
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
	if(argc < 5){
		fprintf(stderr, "usage: fbprobe <x> <y> <w> <h>\n");
		return 2;
	}
	int x0 = atoi(argv[1]), y0 = atoi(argv[2]), w = atoi(argv[3]), h = atoi(argv[4]);
	int fd = open("/dev/fb0", O_RDONLY);
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo fix;
	if(fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0 ||
	   v.bits_per_pixel != 32 || w < 1 || h < 1 || x0 < 0 || y0 < 0 ||
	   x0 + w > (int)v.xres || y0 + h > (int)v.yres){
		fprintf(stderr, "fbprobe: bad framebuffer or rectangle\n");
		return 1;
	}
	unsigned char *fb = mmap(NULL, fix.smem_len, PROT_READ, MAP_SHARED, fd, 0);
	if(fb == MAP_FAILED){
		perror("fbprobe: mmap");
		return 1;
	}
	unsigned long r = 0, g = 0, b = 0;
	for(int y = y0; y < y0 + h; y++)
		for(int x = x0; x < x0 + w; x++){
			unsigned char *p = fb + (size_t)(v.yoffset + y) * fix.line_length + (size_t)x * 4;
			r += p[v.red.offset / 8];
			g += p[v.green.offset / 8];
			b += p[v.blue.offset / 8];
		}
	unsigned long n = (unsigned long)w * h;
	printf("%lu %lu %lu\n", r / n, g / n, b / n);
	return 0;
}

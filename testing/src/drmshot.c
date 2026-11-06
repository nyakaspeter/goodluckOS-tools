/* drmshot: the picture on screen as PPM on stdout, read from the buffer the
 * display controller scans out (what a DRM/KMS app like the launcher shows,
 * which /dev/fb0 and fbshot don't see). Root (to map another client's
 * buffer). Plain ioctls, no libdrm.
 *   drmshot [scale-down factor] > shot.ppm */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define DRM_IOWR(nr, t) _IOWR('d', nr, t)

struct mode_card_res { uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders, min_width, max_width, min_height, max_height; };
struct mode_modeinfo { uint32_t clock; uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew, vdisplay, vsync_start, vsync_end, vtotal, vscan;
	uint32_t vrefresh, flags, type; char name[32]; };
struct mode_crtc { uint64_t set_connectors_ptr; uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid; struct mode_modeinfo mode; };
struct mode_fb_cmd { uint32_t fb_id, width, height, pitch, bpp, depth, handle; };
struct mode_map_dumb { uint32_t handle, pad; uint64_t offset; };

#define IOCTL_GETRESOURCES DRM_IOWR(0xA0, struct mode_card_res)
#define IOCTL_GETCRTC DRM_IOWR(0xA1, struct mode_crtc)
#define IOCTL_GETFB DRM_IOWR(0xAD, struct mode_fb_cmd)
#define IOCTL_MAP_DUMB DRM_IOWR(0xB3, struct mode_map_dumb)

static int shot(const char *dev, int f)
{
	int fd = open(dev, O_RDWR);
	if (fd < 0)
		return 0;
	struct mode_card_res res = { 0 };
	if (ioctl(fd, IOCTL_GETRESOURCES, &res) || !res.count_crtcs) {
		close(fd);
		return 0;
	}
	uint32_t crtcs[16] = { 0 };
	uint32_t n = res.count_crtcs < 16 ? res.count_crtcs : 16;
	memset(&res, 0, sizeof(res));
	res.crtc_id_ptr = (uintptr_t)crtcs;
	res.count_crtcs = n;
	ioctl(fd, IOCTL_GETRESOURCES, &res);
	for (uint32_t i = 0; i < n; i++) {
		struct mode_crtc c = { .crtc_id = crtcs[i] };
		if (ioctl(fd, IOCTL_GETCRTC, &c) || !c.fb_id)
			continue;
		struct mode_fb_cmd fb = { .fb_id = c.fb_id };
		if (ioctl(fd, IOCTL_GETFB, &fb) || !fb.handle) {
			fprintf(stderr, "drmshot: GETFB on %s: %s\n", dev, strerror(errno));
			continue;
		}
		struct mode_map_dumb m = { .handle = fb.handle };
		if (ioctl(fd, IOCTL_MAP_DUMB, &m)) {
			fprintf(stderr, "drmshot: MAP_DUMB on %s: %s\n", dev, strerror(errno));
			continue;
		}
		size_t len = (size_t)fb.pitch * fb.height;
		uint8_t *p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, m.offset);
		if (p == MAP_FAILED) {
			fprintf(stderr, "drmshot: mmap: %s\n", strerror(errno));
			continue;
		}
		int w = fb.width / f, h = fb.height / f;
		printf("P6\n%d %d\n255\n", w, h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				const uint8_t *px = p + (size_t)(y * f) * fb.pitch + (size_t)(x * f) * (fb.bpp / 8);
				uint8_t rgb[3];
				if (fb.bpp == 16) {
					uint16_t v = px[0] | px[1] << 8;
					rgb[0] = (v >> 11) << 3; rgb[1] = ((v >> 5) & 63) << 2; rgb[2] = (v & 31) << 3;
				} else {
					rgb[0] = px[2]; rgb[1] = px[1]; rgb[2] = px[0]; /* XRGB8888 */
				}
				fwrite(rgb, 1, 3, stdout);
			}
		munmap(p, len);
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	int f = argc > 1 ? atoi(argv[1]) : 1;
	if (f < 1)
		f = 1;
	const char *devs[] = { "/dev/dri/card1", "/dev/dri/card0", NULL };
	for (int i = 0; devs[i]; i++)
		if (shot(devs[i], f))
			return 0;
	fprintf(stderr, "drmshot: no scanned-out buffer found\n");
	return 1;
}

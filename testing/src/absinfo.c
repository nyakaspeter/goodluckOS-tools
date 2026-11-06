// absinfo: what the console's gamepad reports for its analog axes.
//   absinfo [/dev/input/eventN] [seconds] [fuzz [flat]]
// A fuzz sets that fuzz and flat (default 0) on every axis first (EVIOCSABS, root).
// Prints each ABS axis' range, fuzz and flat (the kernel's dead zone), and
// with a duration also records the raw values seen while someone moves the
// sticks: the distinct values per axis (how fine the steps are).
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static const char *axis_name(int code) {
    switch (code) {
    case ABS_X: return "X"; case ABS_Y: return "Y"; case ABS_Z: return "Z";
    case ABS_RX: return "RX"; case ABS_RY: return "RY"; case ABS_RZ: return "RZ";
    case ABS_HAT0X: return "HAT0X"; case ABS_HAT0Y: return "HAT0Y";
    default: return "?";
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "/dev/input/event2";
    int seconds = argc > 2 ? atoi(argv[2]) : 0;
    int set_fuzz = argc > 3 ? atoi(argv[3]) : -1;  // absinfo DEV SECONDS FUZZ [FLAT]: set them first
    int set_flat = argc > 4 ? atoi(argv[4]) : 0;
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) { perror(path); return 1; }
    char name[128] = "";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    unsigned long bits[(ABS_MAX + 1 + 8 * sizeof(long) - 1) / (8 * sizeof(long))];
    memset(bits, 0, sizeof(bits));
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits);
    printf("%s: %s\n", path, name);
    int have[ABS_MAX + 1]; memset(have, 0, sizeof(have));
    struct input_absinfo info[ABS_MAX + 1];
    for (int code = 0; code <= ABS_MAX; ++code) {
        if (!(bits[code / (8 * sizeof(long))] & (1ul << (code % (8 * sizeof(long)))))) continue;
        if (ioctl(fd, EVIOCGABS(code), &info[code]) != 0) continue;
        if (set_fuzz >= 0) {
            info[code].fuzz = set_fuzz;
            info[code].flat = set_flat;
            if (ioctl(fd, EVIOCSABS(code), &info[code]) != 0) perror("EVIOCSABS");
            ioctl(fd, EVIOCGABS(code), &info[code]);
        }
        have[code] = 1;
        printf("abs %d %s: value %d min %d max %d fuzz %d flat %d resolution %d\n", code, axis_name(code),
               info[code].value, info[code].minimum, info[code].maximum, info[code].fuzz, info[code].flat,
               info[code].resolution);
    }
    if (seconds <= 0) return 0;
    // Distinct values per axis, for ranges up to 64K.
    unsigned char *seen[ABS_MAX + 1]; memset(seen, 0, sizeof(seen));
    long events[ABS_MAX + 1]; memset(events, 0, sizeof(events));
    long outside[ABS_MAX + 1]; memset(outside, 0, sizeof(outside));
    for (int code = 0; code <= ABS_MAX; ++code) {
        if (!have[code]) continue;
        long range = (long)info[code].maximum - info[code].minimum + 1;
        if (range > 0 && range <= 65536) seen[code] = calloc((size_t)range, 1);
    }
    printf("recording %d s: move both sticks slowly in circles and along the axes\n", seconds);
    fflush(stdout);
    time_t end = time(NULL) + seconds;
    while (time(NULL) < end) {
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, 200) <= 0) continue;
        struct input_event ev[64];
        ssize_t n = read(fd, ev, sizeof(ev));
        for (ssize_t i = 0; i < n / (ssize_t)sizeof(ev[0]); ++i) {
            if (ev[i].type != EV_ABS || ev[i].code > ABS_MAX || !have[ev[i].code]) continue;
            ++events[ev[i].code];
            // The GA36-MB reports past its declared range (223 seen with max 213).
            if (seen[ev[i].code] && ev[i].value >= info[ev[i].code].minimum &&
                ev[i].value <= info[ev[i].code].maximum)
                seen[ev[i].code][ev[i].value - info[ev[i].code].minimum] = 1;
            else if (seen[ev[i].code]) ++outside[ev[i].code];
        }
    }
    for (int code = 0; code <= ABS_MAX; ++code) {
        if (!have[code] || !seen[code]) continue;
        long range = (long)info[code].maximum - info[code].minimum + 1, distinct = 0, lo = -1, hi = -1;
        for (long v = 0; v < range; ++v) if (seen[code][v]) { ++distinct; if (lo < 0) lo = v; hi = v; }
        printf("abs %d %s: %ld events, %ld distinct values, %ld outside the declared range", code,
               axis_name(code), events[code], distinct, outside[code]);
        if (distinct > 0) printf(", seen %ld..%ld", lo + info[code].minimum, hi + info[code].minimum);
        if (distinct > 0 && distinct <= 48) {
            printf(":");
            for (long v = 0; v < range; ++v) if (seen[code][v]) printf(" %ld", v + info[code].minimum);
        }
        printf("\n");
    }
    return 0;
}

// vpad: a virtual clone of the pad (same name, ids, buttons and axes, made
// with uinput), to hold buttons the way a hand does. The real pad's driver
// reports its true state on every poll, so presses written into its node
// (padpress) are released within milliseconds and two buttons can never be
// held together; a clone has no driver behind it. SDL gives it the real
// pad's mapping (same GUID). Start it before the game, it goes away on exit.
//   vpad <ms> <buttons> <ms held> [<ms> <buttons> <ms held> ...]
// waits <ms>, holds the buttons (a+b: pressed 100 ms apart), releases, next.
// Buttons as padpress; lsup, lsdown, lsleft, lsright hold the left stick at
// its end. Device cloned: $PADPRESS_DEV or /dev/input/event2.
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

static int ufd;

static void
emit(int type, int code, int value)
{
	struct input_event ev;
	memset(&ev, 0, sizeof ev);
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;
	if(write(ufd, &ev, sizeof ev) != sizeof ev)
		perror("vpad: write");
}

static const struct { const char *name; int code; } buttons[] = {
	{"a", BTN_SOUTH}, {"b", BTN_EAST}, {"y", BTN_NORTH}, {"x", BTN_WEST},
	{"l", BTN_TL}, {"r", BTN_TR}, {"l2", BTN_TL2}, {"r2", BTN_TR2},
	{"select", BTN_SELECT}, {"start", BTN_START}, {"menu", BTN_MODE},
	{"up", BTN_DPAD_UP}, {"down", BTN_DPAD_DOWN}, {"left", BTN_DPAD_LEFT}, {"right", BTN_DPAD_RIGHT},
};

#define BIT(a, i) ((a)[(i) / 8] & (1 << ((i) % 8)))

int
main(int argc, char **argv)
{
	const char *dev = getenv("PADPRESS_DEV");
	int rfd = open(dev ? dev : "/dev/input/event2", O_RDONLY);
	ufd = open("/dev/uinput", O_WRONLY);
	if(rfd < 0 || ufd < 0 || argc < 4){
		fprintf(stderr, "usage: vpad <ms> <button>[+<button>...] <ms held> ... (needs the pad and /dev/uinput)\n");
		return 1;
	}
	struct uinput_user_dev ud;
	memset(&ud, 0, sizeof ud);
	ioctl(rfd, EVIOCGNAME(sizeof ud.name), ud.name);
	ioctl(rfd, EVIOCGID, &ud.id);
	unsigned char keys[KEY_MAX / 8 + 1], abs[ABS_MAX / 8 + 1];
	memset(keys, 0, sizeof keys);
	memset(abs, 0, sizeof abs);
	ioctl(rfd, EVIOCGBIT(EV_KEY, sizeof keys), keys);
	ioctl(rfd, EVIOCGBIT(EV_ABS, sizeof abs), abs);
	ioctl(ufd, UI_SET_EVBIT, EV_KEY);
	ioctl(ufd, UI_SET_EVBIT, EV_ABS);
	ioctl(ufd, UI_SET_EVBIT, EV_SYN);
	for(int i = 0; i < KEY_MAX; i++)
		if(BIT(keys, i))
			ioctl(ufd, UI_SET_KEYBIT, i);
	for(int i = 0; i < ABS_MAX; i++)
		if(BIT(abs, i)){
			struct input_absinfo info;
			ioctl(ufd, UI_SET_ABSBIT, i);
			if(ioctl(rfd, EVIOCGABS(i), &info) == 0){
				ud.absmin[i] = info.minimum;
				ud.absmax[i] = info.maximum;
				ud.absfuzz[i] = info.fuzz;
				ud.absflat[i] = info.flat;
			}
		}
	if(write(ufd, &ud, sizeof ud) != sizeof ud || ioctl(ufd, UI_DEV_CREATE) < 0){
		perror("vpad: create");
		return 1;
	}
	fprintf(stderr, "vpad: clone of '%s' %04x:%04x:%04x:%04x\n", ud.name, ud.id.bustype, ud.id.vendor, ud.id.product, ud.id.version);
	for(int a = 1; a + 2 < argc; a += 3){
		usleep(atoi(argv[a]) * 1000);
		int codes[8], n = 0;
		char names[128];
		snprintf(names, sizeof names, "%s", argv[a + 1]);
		/* lsup lsdown lsleft lsright: the left stick held at its end */
		int axes[4], ends[4], na = 0;
		for(char *tok = strtok(names, "+"); tok && n < 8; tok = strtok(NULL, "+")){
			for(size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++)
				if(strcmp(tok, buttons[i].name) == 0)
					codes[n++] = buttons[i].code;
			if(strncmp(tok, "ls", 2) == 0 && na < 4){
				int ax = (strcmp(tok, "lsup") == 0 || strcmp(tok, "lsdown") == 0) ? ABS_Y : ABS_X;
				int low = strcmp(tok, "lsup") == 0 || strcmp(tok, "lsleft") == 0;
				axes[na] = ax;
				ends[na++] = low ? ud.absmin[ax] : ud.absmax[ax];
			}
		}
		for(int i = 0; i < na; i++)
			emit(EV_ABS, axes[i], ends[i]);
		if(na)
			emit(EV_SYN, SYN_REPORT, 0);
		for(int i = 0; i < n; i++){
			if(i > 0)
				usleep(100 * 1000);
			emit(EV_KEY, codes[i], 1);
			emit(EV_SYN, SYN_REPORT, 0);
		}
		usleep(atoi(argv[a + 2]) * 1000);
		for(int i = n - 1; i >= 0; i--)
			emit(EV_KEY, codes[i], 0);
		for(int i = 0; i < na; i++)
			emit(EV_ABS, axes[i], (ud.absmin[axes[i]] + ud.absmax[axes[i]]) / 2);
		emit(EV_SYN, SYN_REPORT, 0);
	}
	usleep(500 * 1000);
	ioctl(ufd, UI_DEV_DESTROY);
	return 0;
}

// padpress: press buttons or move sticks on the GA36-MB pad from a shell, by
// writing input events to its evdev node (the kernel passes them to every
// reader as if the pad sent them). For driving games without touching them.
//   padpress <button> [ms]        press for ms (default 150), then release
//   padpress stick <axis> <value> [ms]   axis x|y|rx|ry, then back to centre
// Buttons: a b x y (positions: a = bottom, as SDL maps them) l r l2 r2 menu;
// several joined with + are held together (select+x)
//          select start up down left right
// Device: $PADPRESS_DEV or /dev/input/event2.
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static int fd;

static void
emit(int type, int code, int value)
{
	struct input_event ev;
	memset(&ev, 0, sizeof ev);
	gettimeofday(&ev.time, NULL);
	ev.type = type;
	ev.code = code;
	ev.value = value;
	if(write(fd, &ev, sizeof ev) != sizeof ev)
		perror("padpress: write");
}

static void
sync_events(void)
{
	emit(EV_SYN, SYN_REPORT, 0);
}

static const struct { const char *name; int code; } buttons[] = {
	{"a", BTN_SOUTH}, {"b", BTN_EAST}, {"y", BTN_NORTH}, {"x", BTN_WEST},
	{"l", BTN_TL}, {"r", BTN_TR}, {"l2", BTN_TL2}, {"r2", BTN_TR2},
	{"select", BTN_SELECT}, {"start", BTN_START}, {"menu", BTN_MODE},
	{"up", BTN_DPAD_UP}, {"down", BTN_DPAD_DOWN}, {"left", BTN_DPAD_LEFT}, {"right", BTN_DPAD_RIGHT},
};

static const struct { const char *name; int code; } axes[] = {
	{"x", ABS_X}, {"y", ABS_Y}, {"rx", ABS_RX}, {"ry", ABS_RY},
};

int
main(int argc, char **argv)
{
	const char *dev = getenv("PADPRESS_DEV");
	fd = open(dev ? dev : "/dev/input/event2", O_WRONLY);
	if(fd < 0){
		perror("padpress: open");
		return 1;
	}
	if(argc >= 4 && strcmp(argv[1], "stick") == 0){
		for(size_t i = 0; i < sizeof axes / sizeof axes[0]; i++)
			if(strcmp(argv[2], axes[i].name) == 0){
				struct input_absinfo info;
				int centre = 0;
				if(ioctl(fd, EVIOCGABS(axes[i].code), &info) == 0)
					centre = (info.minimum + info.maximum) / 2;
				emit(EV_ABS, axes[i].code, atoi(argv[3]));
				sync_events();
				usleep((argc > 4 ? atoi(argv[4]) : 150) * 1000);
				emit(EV_ABS, axes[i].code, centre);
				sync_events();
				return 0;
			}
	} else if(argc >= 2){
		// One button, or several held together: select+x. They go down 100 ms
		// apart in the order given and come up together.
		int codes[8], n = 0;
		char names[128];
		snprintf(names, sizeof names, "%s", argv[1]);
		for(char *tok = strtok(names, "+"); tok && n < 8; tok = strtok(NULL, "+")){
			int code = -1;
			for(size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++)
				if(strcmp(tok, buttons[i].name) == 0)
					code = buttons[i].code;
			if(code < 0){
				n = 0;
				break;
			}
			codes[n++] = code;
		}
		if(n > 0){
			for(int i = 0; i < n; i++){
				if(i > 0)
					usleep(100 * 1000);
				emit(EV_KEY, codes[i], 1);
				sync_events();
			}
			usleep((argc > 2 ? atoi(argv[2]) : 150) * 1000);
			for(int i = n - 1; i >= 0; i--)
				emit(EV_KEY, codes[i], 0);
			sync_events();
			return 0;
		}
	}
	fprintf(stderr, "usage: padpress <button>[+<button>...] [ms] | padpress stick <x|y|rx|ry> <value> [ms]\n");
	return 2;
}

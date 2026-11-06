# goodluckOS-tools — reaching a GA36-MB on goodluckOS from a Mac

The shared development tooling for the ports: a root shell over the
console's USB serial port, file copies over MTP, the "is anyone playing?" check
that comes before every command that changes anything, the console-side test
tools, and the boot-region flasher. None of it ships: a released game folder
carries everything it needs on its own.

| File | What |
|---|---|
| `console.py '<cmd>'` | Run a command on the console as root over its USB serial port. `GLOS_TIMEOUT=<s>` for long ones. Standard library only. |
| `console-idle.sh` | Exits 0 when nobody is playing: no app running under appd and no pad input for 5 s. Use it as `console-idle.sh \|\| exit 1; <command>` (an `&&` chain lets a following `;` run anyway). |
| `mtp_send.py / <file>…` | Copy files to the top of the console's HOME over USB (MTP). Never overwrites. Needs libmtp (`brew install libmtp`). |
| `mtp_get.py <name> <dest>` | Copy a file from the top of HOME to the Mac. |
| `testing-sync.sh <id>` | Copy `testing/bin/` into `/home/player/ports/<id>/glos/testing/` on the console for a test run. |
| `testing-clean.sh <id>` | Remove it again (and a bench's `start.bench.sh`). |
| `shot.sh <id> [file.png] [factor]` | Screenshot of the console's framebuffer through that folder's `fbshot`. |
| `testing/` | The console-side test tools: `src/` (C), `bin/` (armhf builds), `build.sh` (cross-compiles them in a ports' builder image). `fbshot`, `fbprobe`, `padpress`, `psample`, `stackscan`, `absinfo`, `memscan`, `pin`, `ioctlcount.so`, `divcount.so`. |
| `flash-boot.sh /dev/diskN <image>` | Write a boot image to the card's boot region (sector 172032), with a backup and read-back check. |

MTP traps: macOS's `ptpcamerad` grabs the console after the first transfer of
a session (`pkill -9 -x ptpcamerad` right before each one) and Photos.app holds
it while open. The console's shell appends to what `console.py` sends, so a
command must not end in `&`. `console.py` runs one command at a time: a second
one sent while another runs garbles both, and a killed `console.py` leaves its
command running on the console.

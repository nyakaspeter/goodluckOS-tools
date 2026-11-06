#!/usr/bin/env python3
"""Run a shell command on the goodluckOS console over its USB serial port.

Usage: python3 console.py "uname -a"
Logs in as root/root if a login prompt appears. Standard library only.
"""
import glob
import os
import select
import sys
import termios
import time

BAUD = termios.B115200
MARK = "__GLOS_DONE__"


def open_port():
    ports = glob.glob("/dev/cu.usbmodem*")
    if not ports:
        sys.exit("No /dev/cu.usbmodem* port found. Is the console connected?")
    fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0                                   # iflag
    attrs[1] = 0                                   # oflag
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0                                   # lflag: raw
    attrs[4] = attrs[5] = BAUD
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_until(fd, needles, timeout):
    buf = ""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            buf += os.read(fd, 4096).decode(errors="replace")
            if any(n in buf for n in needles):
                break
    return buf


def send(fd, text):
    os.write(fd, text.encode())


def main():
    cmd = " ".join(sys.argv[1:]) or "uname -a"
    timeout = float(os.environ.get("GLOS_TIMEOUT", "30"))
    fd = open_port()
    send(fd, "\r")
    out = read_until(fd, ["login:", "# ", "$ "], 5)
    if "login:" in out:
        send(fd, "root\r")
        out = read_until(fd, ["Password:", "# "], 5)
        if "Password:" in out:
            send(fd, "root\r")
            read_until(fd, ["# ", "incorrect"], 10)
    # Turn off echo so long commands don't wrap into the output.
    send(fd, "stty -echo cols 1000\r")
    read_until(fd, [], 0.5)                        # drain
    # Split the marker with quotes so the echoed command line doesn't match it.
    send(fd, f"{cmd}; echo __GLOS_''DONE__$?\r")
    out = read_until(fd, [MARK], timeout)
    out += read_until(fd, ["\n"], 1)
    os.close(fd)
    # Drop the echoed command line and the marker.
    lines = out.replace("\r", "").split("\n")
    # Skip the echoed command if echo was still on.
    start = next((i + 1 for i, l in enumerate(lines) if "__GLOS_''DONE__" in l), 0)
    body, status = [], None
    for line in lines[start:]:
        if MARK in line:
            before, _, after = line.partition(MARK)
            if before:
                body.append(before)
            status = after.strip()
            break
        body.append(line)
    print("\n".join(body).rstrip())
    if status is None:
        sys.exit("Timed out waiting for the command to finish.")
    sys.exit(int(status) if status.isdigit() else 1)


if __name__ == "__main__":
    main()

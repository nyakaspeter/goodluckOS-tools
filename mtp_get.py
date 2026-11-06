#!/usr/bin/env python3
"""Copy files from the top of the goodluckOS console's HOME to the Mac (MTP).

Usage: python3 mtp_get.py <name on console> [<local path or dir>]
Put the file in /home/player first (e.g. with console.py 'cp ... /home/player/').
"""
import os
import sys

import mtp_send as m

m.mtp.LIBMTP_Get_File_To_File.argtypes = [
    m.C.c_void_p, m.C.c_uint32, m.C.c_char_p, m.PROGRESS, m.C.c_void_p]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    name = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else name
    if os.path.isdir(out):
        out = os.path.join(out, name)
    dev = m.open_device()
    if not dev:
        sys.exit("Console not found over MTP. Is it connected and booted?")
    try:
        item = next((f.item_id for f in m.listing(dev, 0) if f.filename.decode() == name), None)
        if item is None:
            sys.exit(f"'{name}' not found at the top of HOME.")
        print(f"get   {name} -> {out}")
        rc = m.mtp.LIBMTP_Get_File_To_File(dev, item, out.encode(), m.progress, None)
        print()
        if rc != 0:
            m.mtp.LIBMTP_Dump_Errorstack(dev)
            sys.exit(f"FAILED {name}")
    finally:
        m.mtp.LIBMTP_Release_Device(dev)


if __name__ == "__main__":
    main()

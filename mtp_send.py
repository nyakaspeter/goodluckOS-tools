#!/usr/bin/env python3
"""Copy files to the goodluckOS console's HOME storage over MTP.

Usage: python3 mtp_send.py roms/snes game1.smc game2.smc ...
       python3 mtp_send.py / file.zip   (top of HOME)

Uses libmtp directly (brew install libmtp) because mtp-sendfile can't
resolve folder paths on this device. Files that already exist in the
destination folder are skipped.
"""
import ctypes as C
import os
import sys

LIB = "/opt/homebrew/lib/libmtp.dylib"
FILETYPE_FOLDER = 0
FILETYPE_UNKNOWN = 44


class File(C.Structure):
    pass


File._fields_ = [
    ("item_id", C.c_uint32),
    ("parent_id", C.c_uint32),
    ("storage_id", C.c_uint32),
    ("filename", C.c_char_p),
    ("filesize", C.c_uint64),
    ("modificationdate", C.c_long),
    ("filetype", C.c_int),
    ("next", C.POINTER(File)),
]

PROGRESS = C.CFUNCTYPE(C.c_int, C.c_uint64, C.c_uint64, C.c_void_p)

mtp = C.CDLL(LIB)
mtp.LIBMTP_Detect_Raw_Devices.argtypes = [C.POINTER(C.c_void_p), C.POINTER(C.c_int)]
mtp.LIBMTP_Open_Raw_Device_Uncached.argtypes = [C.c_void_p]
mtp.LIBMTP_Open_Raw_Device_Uncached.restype = C.c_void_p
mtp.LIBMTP_Get_Files_And_Folders.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32]
mtp.LIBMTP_Get_Files_And_Folders.restype = C.POINTER(File)
mtp.LIBMTP_Get_Storage.argtypes = [C.c_void_p, C.c_int]
mtp.LIBMTP_new_file_t.restype = C.POINTER(File)
mtp.LIBMTP_Send_File_From_File.argtypes = [
    C.c_void_p, C.c_char_p, C.POINTER(File), PROGRESS, C.c_void_p]
mtp.LIBMTP_Release_Device.argtypes = [C.c_void_p]
mtp.LIBMTP_Dump_Errorstack.argtypes = [C.c_void_p]
mtp.LIBMTP_Clear_Errorstack.argtypes = [C.c_void_p]


class DeviceHead(C.Structure):
    """The first fields of LIBMTP_mtpdevice_t, up to its storage list."""
    _fields_ = [("object_bitsize", C.c_uint8), ("params", C.c_void_p),
                ("usbinfo", C.c_void_p), ("storage", C.c_void_p)]


storage_id = 0


def open_device():
    """The console, uncached: libmtp's cached open (LIBMTP_Get_First_Device)
    reads every object on the card first, minutes with 30,000+ files."""
    mtp.LIBMTP_Init()
    raw, n = C.c_void_p(), C.c_int()
    global storage_id
    if mtp.LIBMTP_Detect_Raw_Devices(C.byref(raw), C.byref(n)) != 0 or n.value < 1:
        return None
    dev = mtp.LIBMTP_Open_Raw_Device_Uncached(raw)
    # the console's MTP server answers listings only for its real storage
    # id (HOME), not for 0 = all storages
    if dev and mtp.LIBMTP_Get_Storage(dev, 0) == 0:
        storage = C.cast(dev, C.POINTER(DeviceHead)).contents.storage
        if storage:
            storage_id = C.cast(storage, C.POINTER(C.c_uint32)).contents.value
    return dev


def listing(dev, parent):
    """One folder's entries, without reading the whole card:
    Bully's 30,000 loose files made a full listing take minutes."""
    # the top of HOME is 0 as a parent when sending, 0xFFFFFFFF when listing
    node = mtp.LIBMTP_Get_Files_And_Folders(dev, storage_id, parent or 0xFFFFFFFF)
    while node:
        yield node.contents
        node = node.contents.next


def find_folder(dev, parts):
    """Walk down from the top of HOME, one folder listing per path component,
    matching names case-insensitively."""
    folder_id = 0
    for part in parts:
        for f in listing(dev, folder_id):
            if f.filetype == FILETYPE_FOLDER and f.filename.decode().lower() == part.lower():
                folder_id = f.item_id
                break
        else:
            return None
    return folder_id


@PROGRESS
def progress(sent, total, _):
    pct = sent * 100 // total if total else 100
    print(f"\r  {pct:3d}%", end="", flush=True)
    return 0


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    dest, paths = sys.argv[1].strip("/"), sys.argv[2:]
    dev = open_device()
    if not dev:
        sys.exit("Console not found over MTP. Is it connected and booted?")
    failed = 0
    try:
        folder_id = find_folder(dev, dest.split("/")) if dest else 0  # 0: top of HOME
        if folder_id is None:
            sys.exit(f"Folder '{dest}' not found on the console.")
        present = {f.filename.decode() for f in listing(dev, folder_id)}
        for path in paths:
            name = os.path.basename(path)
            if not os.path.isfile(path):
                print(f"skip  {path} (not a file)")
                continue
            if name in present:
                print(f"skip  {name} (already on console)")
                continue
            meta = mtp.LIBMTP_new_file_t()
            meta.contents.filename = name.encode()
            meta.contents.filesize = os.path.getsize(path)
            meta.contents.filetype = FILETYPE_UNKNOWN
            meta.contents.parent_id = folder_id
            meta.contents.storage_id = storage_id
            print(f"send  {name} -> {dest}/")
            rc = mtp.LIBMTP_Send_File_From_File(dev, path.encode(), meta, progress, None)
            print()
            if rc != 0:
                failed += 1
                print(f"FAILED {name}")
                mtp.LIBMTP_Dump_Errorstack(dev)
                mtp.LIBMTP_Clear_Errorstack(dev)
    finally:
        mtp.LIBMTP_Release_Device(dev)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

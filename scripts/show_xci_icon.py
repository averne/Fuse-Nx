#!/usr/bin/env python3

import sys
import fnx
from PIL import Image


def main(argc, argv):
    if (argc != 2):
        print(f"Usage: {argv[0]} xci")
        return 1

    xci = fnx.Xci(argv[1])
    print(f"Xci: cart type: {xci.cart_type:#x}")

    # Open the secure partition, sort entries by decreasing size (the control nca will usually be the second)
    secure_part = xci.get_partition("secure")
    entries = sorted(secure_part.entries.values(), key=lambda entry: entry.size, reverse=True)

    for entry in entries:
        nca = fnx.Nca(secure_part.open(entry))
        if nca.content_type != fnx.Nca.ContentType.CONTROL:
            continue

        for sec in nca.sections:
            if isinstance(sec, fnx.Romfs):
                for f in sec.file_entries.values():
                    if f.name.endswith(".dat"):
                        print(f"Showing {f.name}")
                        Image.open(sec.open(f)).show()
                        return 0

    return 1


if __name__ == "__main__":
    fnx.Keys.init_keys()
    fnx.Keys.init_titlekeys()

    sys.exit(main(len(sys.argv), sys.argv))

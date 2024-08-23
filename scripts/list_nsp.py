#!/usr/bin/env python3

import sys
import fnx
from binascii import hexlify
from PIL import Image


def main(argc, argv):
    if (argc != 2):
        print(f"Usage: {argv[0]} nsp")
        return 1

    nsp = fnx.Pfs(argv[1])
    print(f"Pfs: valid: {nsp.valid}, num files: {nsp.num_entries}")

    for entry_name, entry in nsp.entries.items():
        print(f"{entry_name}: offset: {entry.offset}, size: {entry.size}")

        nca = fnx.Nca(nsp.open(entry))
        if not nca.valid:
            continue

        print(f"  Distribution type: {fnx.Nca.DistributionType(nca.distribution_type).name}, \
content type: {fnx.Nca.ContentType(nca.content_type).name}, title id: {nca.title_id:016x}, \
sdk version: {'.'.join(map(str, nca.sdk_ver))}, rights id: {nca.rights_id.hex()}")

        for sec in nca.sections:
            if isinstance(sec, fnx.Pfs):
                print(f"  Pfs section ({sec.num_entries} entries):")
                for name, entry in sec.entries.items():
                    print(f"    {name}: offset: {entry.offset}, size: {entry.size}")
            elif isinstance(sec, fnx.Romfs):
                print(f"  Romfs section ({sec.num_dir_entries} directories, {sec.num_file_entries} files):")
                for path, entry in sec.dir_entries.items():
                    print(f"    {path}:")
                    for c in entry.children:
                        print(f"      Child: {c.name}: {len(c.children)} children, {len(c.files)} files")
                    for f in entry.files:
                        print(f"      File: {f.name}, offset: {f.offset}, size: {f.size}")

    return 0


if __name__ == "__main__":
    fnx.Keys.init_keys()
    fnx.Keys.init_titlekeys()

    sys.exit(main(len(sys.argv), sys.argv))

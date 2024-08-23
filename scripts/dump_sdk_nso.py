#!/usr/bin/env python3

import sys, shutil
import fnx

def find_sdk_in_nca(nca):
    if nca.content_type != fnx.Nca.ContentType.Program:
        return

    for sec in nca.sections:
        if isinstance(sec, fnx.Pfs):
            try:
                sdk = sec.open("sdk")
                print(f"Sdk version: {'.'.join(map(str, nca.sdk_ver))}")
                return sdk
            except FileNotFoundError:
                continue


def find_sdk_in_nsp(nsp):
    entries = sorted(nsp.entries.values(), key=lambda entry: entry.size, reverse=True)

    for entry in entries:
        if entry.name.endswith(".nca"):
            nca = fnx.Nca(nsp.open(entry))
            if (sdk := find_sdk_in_nca(nca)) is not None:
                return sdk


def find_sdk_in_xci(xci):
    return find_sdk_in_nsp(xci.get_partition("secure")) # Since the API for HFS/PFS is the same we can just pass the secure HFS


def main(argc, argv):
    if (argc < 2):
        print(f"Usage: {argv[0]} file...")
        return 1

    for arg in argv[1:]:
        f = fnx.open_match(arg)
        print(f"Searching for sdk binary in {f.__class__.__name__}")

        try:
            if isinstance(f, fnx.Nca):
                sdk = find_sdk_in_nca(f)
            elif isinstance(f, fnx.Pfs):
                sdk = find_sdk_in_nsp(f)
            elif isinstance(f, fnx.Xci):
                sdk = find_sdk_in_xci(f)
            else:
                sdk = None

            if sdk is not None:
                with open("sdk.nso", "wb") as dst:
                    shutil.copyfileobj(sdk, dst)
            else:
                print("Sdk binary not found")
        except Exception as e:
            print(f"Error on {arg}:", e)

    return 0


if __name__ == "__main__":
    fnx.Keys.init_keys()
    fnx.Keys.init_titlekeys()

    sys.exit(main(len(sys.argv), sys.argv))

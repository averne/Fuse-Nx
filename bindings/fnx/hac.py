#!/usr/bin/env python3

# Copyright (C) 2020 averne
#
# This file is part of fuse-nx.
#
# fuse-nx is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# fuse-nx is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with fuse-nx.  If not, see <http://www.gnu.org/licenses/>.

import os, io, re
import enum
import fnxbinds
from typing import Dict, Union


class File(io.RawIOBase):
    def __init__(self, *args):
        if isinstance(args[0], fnxbinds.FileBase):
            self.base = args[0]
        elif isinstance(args[0], io.IOBase):
            f = args[0]
            pos = f.tell()
            f.seek(0, io.SEEK_END)
            size = f.tell()
            f.seek(pos, io.SEEK_SET)

            self.base = fnxbinds.FileBase(f, size)
        else:
            self.base = fnxbinds.FileBase(*args)

    def close(self):
        pass

    @property
    def size(self) -> int:
        return self.base.size()

    def seek(self, where: int, whence: int=io.SEEK_SET):
        self.base.seek(where, whence)

    def tell(self) -> int:
        return self.base.tell()

    def readall(self):
        b = bytes(self.size)
        self.readinto(b)
        return b

    def readinto(self, b: bytes) -> int:
        return self.base.read(b)

    def write(self, b: bytes) -> int:
        return self.base.write(b)

    def parent_offset(self):
        return self.base.parent_offset()

    def clone(self):
        return File(self.base.clone())


class Format(enum.IntEnum):
    Pfs     = fnxbinds.FORMAT_PFS
    Hfs     = fnxbinds.FORMAT_HFS
    Romfs   = fnxbinds.FORMAT_ROMFS
    Nca     = fnxbinds.FORMAT_NCA
    Xci     = fnxbinds.FORMAT_XCI
    Unknown = fnxbinds.FORMAT_UNK


class Pfs:
    """ Class representing a Pfs (Pfs0/Nsp) """

    def __init__(self, file_or_path: Union[File, str, fnxbinds.Pfs], parse: bool=False):
        """ Creates the Pfs, arguments: path to the file, or File object """
        if isinstance(file_or_path, fnxbinds.Pfs):
            self.base = file_or_path
        elif isinstance(file_or_path, File):
            self.base = fnxbinds.Pfs(file_or_path.base)
        else:
            self.base = fnxbinds.Pfs(File(file_or_path, "rb").base)

        self.parsed = False
        if parse:
            self.parse()

    @property
    def valid(self) -> bool:
        """ Sanity check on the magicnum of the Pfs """
        return self.base.is_valid()

    def parse(self) -> bool:
        """ Parses the Pfs and populate the entry dict
        Calling this directly is usually not necessary """
        if not self.valid:
            raise RuntimeError("Invalid Pfs")
        if not self.base.parse():
            raise RuntimeError("Failed to parse the Pfs")
        self._entries = self.base.get_entries()
        self.parsed = True

    @property
    def num_entries(self) -> int:
        """ Returns the number of entries contained in the Pfs """
        if not self.parsed:
            self.parse()
        return len(self._entries)

    @property
    def entries(self) -> Dict[str, fnxbinds.PfsEntry]:
        """ Returns the entries contained in the Pfs
        Entry layout:
            class PfsEntry:
                name: str
                offset: int
                size: int """
        if not self.parsed:
            self.parse()
        return self._entries

    def open(self, entry: Union[str, fnxbinds.PfsEntry]) -> File:
        """ Opens an entry and returns a file object, arguments: name or entry object """
        if not self.parsed:
            self.parse()
        if isinstance(entry, str):
            if entry not in self._entries.keys():
                raise FileNotFoundError("Entry does not exist")
            entry = self._entries[entry]
        return File(self.base.open(entry))


class Hfs:
    """ Class representing an Hfs """

    def __init__(self, file_or_path: Union[File, str, fnxbinds.Hfs], parse: bool=False):
        """ Creates the Hfs, arguments: path to the file, or File object """
        if isinstance(file_or_path, fnxbinds.Hfs):
            self.base = file_or_path
        elif isinstance(file_or_path, File):
            self.base = fnxbinds.Hfs(file_or_path.base)
        else:
            self.base = fnxbinds.Hfs(File(file_or_path, "rb").base)

        self.parsed = False
        if parse:
            self.parse()

    @property
    def valid(self) -> bool:
        """ Sanity check on the magicnum of the Hfs """
        return self.base.is_valid()

    def parse(self) -> bool:
        """ Parses the Hfs and populate the entry dict
        Calling this directly is usually not necessary """
        if not self.valid:
            raise RuntimeError("Invalid Hfs")
        if not self.base.parse():
            raise RuntimeError("Failed to parse the Hfs")
        self._entries = self.base.get_entries()
        self.parsed = True

    @property
    def num_entries(self) -> int:
        """ Returns the number of entries contained in the Hfs """
        if not self.parsed:
            self.parse()
        return len(self._entries)

    @property
    def entries(self) -> Dict[str, fnxbinds.HfsEntry]:
        """ Returns the entries contained in the Hfs
        Entry layout:
            class HfsEntry:
                name: str
                offset: int
                size: int """
        if not self.parsed:
            self.parse()
        return self._entries

    def open(self, entry: Union[str, fnxbinds.HfsEntry]) -> File:
        """ Opens an entry and returns a file object, arguments: name or entry object """
        if not self.parsed:
            self.parse()
        if isinstance(entry, str):
            if entry not in self._entries.keys():
                raise FileNotFoundError("Entry does not exist")
            entry = self._entries[entry]
        return File(self.base.open(entry))


class Romfs:
    """ Class representing a RomFs """

    def __init__(self, file_or_path: Union[File, str, fnxbinds.Romfs], parse: bool=False):
        """ Creates the RomFs, arguments: path to the file, or File object """
        if isinstance(file_or_path, fnxbinds.Romfs):
            self.base = file_or_path
        elif isinstance(file_or_path, File):
            self.base = fnxbinds.Romfs(file_or_path.base)
        else:
            self.base = fnxbinds.Romfs(File(file_or_path, "rb").base)

        self.parsed = False
        if parse:
            self.parse()

    @property
    def valid(self) -> bool:
        """ Sanity check on the magicnum of the RomFs """
        return self.base.is_valid()

    def parse(self) -> bool:
        """ Parses the RomFs and populate the file and directory dicts
        Calling this directly is usually not necessary """
        if not self.valid:
            raise RuntimeError("Invalid RomFs")
        if not self.base.parse():
            raise RuntimeError("Failed to parse the RomFs")
        self._file_entries, self._dir_entries = self.base.get_entries()
        self.parsed = True

    @property
    def num_file_entries(self) -> int:
        """ Returns the number of file entries contained in the RomFs """
        if not self.parsed:
            self.parse()
        return len(self._file_entries)

    @property
    def file_entries(self) -> Dict[str, fnxbinds.RomfsFileEntry]:
        """ Returns the file entries contained in the RomFs
        Entry layout:
            class RomfsFileEntry:
                name: str
                parent: RomfsDirEntry (see below)
                offset: int
                size: int"""
        if not self.parsed:
            self.parse()
        return self._file_entries

    @property
    def num_dir_entries(self) -> int:
        """ Returns the number of directory entries contained in the RomFs """
        if not self.parsed:
            self.parse()
        return len(self._dir_entries)

    @property
    def dir_entries(self) -> Dict[str, fnxbinds.RomfsDirEntry]:
        """ Returns the directory entries contained in the RomFs
        Entry layout:
            class RomfsDirEntry:
                name: str
                parent: RomfsDirEntry
                children: List[RomfsDirEntry]
                files: List[RomfsFileEntry] (see above) """
        if not self.parsed:
            self.parse()
        return self._dir_entries

    def open(self, entry: Union[str, fnxbinds.RomfsFileEntry]) -> File:
        """ Opens an entry and returns a file object, arguments: name or entry object """
        if not self.parsed:
            self.parse()
        if isinstance(entry, str):
            if entry not in self._file_entries.keys():
                raise FileNotFoundError("Entry does not exist")
            entry = self._file_entries[entry]
        return File(self.base.open(entry))


class Nca:
    """ Class representing an Nca """

    class DistributionType(enum.IntEnum):
        Download = 0
        Gamecard = 1

    class ContentType(enum.IntEnum):
        Program    = 0
        Meta       = 1
        Control    = 2
        Manual     = 3
        Data       = 4
        PublicData = 5

    def __init__(self, file_or_path: Union[File, str], parse: bool=False):
        """ Creates the Nca, arguments: path to the file, or File object """
        if isinstance(file_or_path, File):
            file = file_or_path
        else:
            file = File(file_or_path, "rb")
        self.base = fnxbinds.Nca(file.base)

        self.parsed = False
        if parse:
            self.parse()

    @property
    def valid(self) -> bool:
        """ Sanity check on the magicnum of the Nca """
        return self.base.is_valid()

    def parse(self) -> bool:
        """ Parses the Nca and populate the section list
        Calling this directly is usually not necessary """
        if not self.valid:
            raise RuntimeError("Invalid Nca")
        if not self.base.parse():
            raise RuntimeError("Failed to parse the Nca")
        self._sections = []
        for sec in self.base.get_sections():
            if isinstance(sec, fnxbinds.Pfs):
                self._sections.append(Pfs(sec))
            else:
                self._sections.append(Romfs(sec))
        self.parsed = True

    @property
    def distribution_type(self) -> DistributionType:
        """ Returns the distribution type from the header of the Nca """
        return Nca.DistributionType(self.base.get_distribution_type())

    @property
    def content_type(self) -> ContentType:
        """ Returns the content type from the header of the Nca """
        return Nca.ContentType(self.base.get_content_type())

    @property
    def size(self) -> int:
        """ Returns the size from the header of the Nca """
        return self.base.get_size()

    @property
    def title_id(self) -> int:
        """ Returns the title id (program id) from the header of the Nca """
        return self.base.get_title_id()

    @property
    def sdk_ver(self) -> tuple[int, int, int, int]:
        """ Returns the sdk version from the header of the Nca """
        return self.base.get_sdk_ver()

    @property
    def rights_id(self) -> bytes:
        """ Returns the rights id from the header of the Nca """
        return self.base.get_rights_id()

    @property
    def num_sections(self):
        """ Returns the number of sections contained in the Nca """
        return len(self._sections)

    @property
    def sections(self):
        """ Returns a list of sections contained in the Nca, each either a Pfs or Romfs object """
        if not self.parsed:
            self.parse()
        return self._sections

    @property
    def section_types(self):
        """ Returns a list of types within the Nca """
        return [Format(_) for _ in self.base.get_section_types()]

    @property
    def section_bounds(self):
        """ Returns a list of section boundaries (offset/size) within the Nca """
        return self.base.get_section_bounds()


class Xci:
    """ Class representing an Xci """

    class CartType(enum.IntEnum):
        Gb1  = 0xfa
        Gb2  = 0xf8
        Gb4  = 0xf0
        Gb8  = 0xe0
        Gb16 = 0xe1
        Gb32 = 0xe2

    def __init__(self, file_or_path: Union[File, str], parse: bool=False):
        """ Creates the Xci, arguments: path to the file, or File object """
        if isinstance(file_or_path, File):
            file = file_or_path
        else:
            file = File(file_or_path, "rb")
        self.base = fnxbinds.Xci(file.base)

        self.parsed = False
        if parse:
            self.parse()

    @property
    def valid(self) -> bool:
        """ Sanity check on the magicnum of the Xci """
        return self.base.is_valid()

    def parse(self) -> bool:
        """ Parses the Xci and populate the partition dict
        Calling this directly is usually not necessary """
        if not self.valid:
            raise RuntimeError("Invalid Xci")
        if not self.base.parse():
            raise RuntimeError("Failed to parse the Xci")
        self._partitions = {}
        for name, part in self.base.get_partitions().items():
            self._partitions[name] = Hfs(part)
        self.parsed = True

    @property
    def cart_type(self) -> CartType:
        return Xci.CartType(self.base.get_cart_type())

    @property
    def num_partitions(self):
        """ Returns the number of partitions contained in the Xci """
        if not self.parsed:
            self.parse()
        return len(self._partitions)

    @property
    def partitions(self):
        """ Returns the partitions contained in the Xci, each an Hfs object """
        if not self.parsed:
            self.parse()
        return self._partitions

    def get_partition(self, name: str) -> Hfs:
        """ Returns a partition in the Xci based on its name as an Hfs object """
        if not self.parsed:
            self.parse()
        if name not in self._partitions.keys():
            raise ValueError("Partition name not found")
        return self._partitions[name]

class Keys:
    class Type(enum.IntEnum):
        Prod  = enum.auto()
        Dev   = enum.auto()
        Title = enum.auto()

    _key_reg = re.compile(r"""^\s*([a-zA-Z0-9_]+)\s* # name
                            =
                            \s*([a-fA-F0-9]+)\s*$ # key""", re.VERBOSE)

    @staticmethod
    def get_default_path(type: Type) -> str:
        """ Gets the default path for a key file type (~/.switch/%s.keys on *nix) """
        if type == Keys.Type.Prod:
            return os.path.join(os.path.expanduser("~"), ".switch", "prod.keys")
        elif type == Keys.Type.Dev:
            return os.path.join(os.path.expanduser("~"), ".switch", "dev.keys")
        elif type == Keys.Type.Title:
            return os.path.join(os.path.expanduser("~"), ".switch", "title.keys")

    @staticmethod
    def init_keys(path_or_type: Union[str, Type]=Type.Prod) -> None:
        """ Initializes the console key list, arguments: path or keyset type """
        if isinstance(path_or_type, Keys.Type):
            if path_or_type == Keys.Type.Title:
                raise ValueError("Trying to init keyset with titlekeys")
            path = Keys.get_default_path(path_or_type)
        else:
            path = path_or_type

        try:
            fp = open(path, "r")
        except FileNotFoundError:
            return

        it = (re.search(Keys._key_reg, l) for l in fp)
        for match in it:
            if match is not None:
                fnxbinds.set_key(match[1], match[2])

    @staticmethod
    def set_key(name: str, key: str) -> None:
        fnxbinds.set_key(name, key)

    @staticmethod
    def init_titlekeys(path: str=None) -> None:
        """ Initializes the title key list, arguments: path or keyset type """
        if path is None:
            path = Keys.get_default_path(Keys.Type.Title)

        try:
            fp = open(path, "r")
        except FileNotFoundError:
            return

        it = (re.search(Keys._key_reg, l) for l in fp)
        for match in it:
            if match is not None:
                fnxbinds.set_titlekey(match[1], match[2])

    @staticmethod
    def set_titlekey(rights_id: str, key: str) -> None:
        fnxbinds.set_titlekey(rights_id, key)

    @staticmethod
    def set_user_titlekey(key: str):
        """ Overrides the titlekeys for any rights id """
        fnxbinds.set_user_titlekey(key)

    @staticmethod
    def remove_user_titlekey() -> None:
        fnxbinds.remove_user_titlekey()


def match(file_or_path: Union[str, File]) -> Format:
    """ Matches a file against its header (checks the magicnum)
    Arguments: path or file object """
    if isinstance(file_or_path, File):
        file = file_or_path
    else:
        file = File(file_or_path, "rb")
    return fnxbinds.match(file.base)


def open_match(file_or_path: Union[str, File]) -> Union[Pfs, Hfs, Romfs, Nca, Xci]:
    """ Matches a file against its header (checks the magicnum) and returns the appropriate object
    Arguments: path or file object """
    if isinstance(file_or_path, File):
        file = file_or_path
    else:
        file = File(file_or_path, "rb")
    fmt = fnxbinds.match(file.base)

    if fmt == Format.Pfs:   return Pfs(file)
    if fmt == Format.Hfs:   return Hfs(file)
    if fmt == Format.Romfs: return Romfs(file)
    if fmt == Format.Nca:   return Nca(file)
    if fmt == Format.Xci:   return Xci(file)

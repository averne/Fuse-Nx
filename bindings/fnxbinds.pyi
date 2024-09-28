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


from typing import List, Tuple, Dict, Union

class FileBase:
    def __init__(self, *args) -> None: ...

    def size(self) -> int: ...

    def seek(self, where: int, whence: int) -> None: ...
    def tell(self) -> int: ...

    def read(self, data: bytes) -> int: ...
    def write(self, data: bytes) -> int: ...

    def parent_offset(self) -> int: ...
    def clone(self) -> FileBase: ...


class PfsEntry:
    name: str = ...
    offset: int = ...
    size: int = ...


class Pfs:
    def __init__(self, base: FileBase) -> None: ...

    def is_valid(self) -> bool: ...
    def parse(self) -> bool: ...

    def get_num_entries(self) -> int: ...
    def get_entries(self) -> Dict[str, PfsEntry]: ...

    def open(self, entry: PfsEntry) -> FileBase: ...


class HfsEntry:
    name: str = ...
    offset: int = ...
    size: int = ...


class Hfs:
    def __init__(self, base: FileBase) -> None: ...

    def is_valid(self) -> bool: ...
    def parse(self) -> bool: ...

    def get_num_entries(self) -> int: ...
    def get_entries(self) -> Dict[str, HfsEntry]: ...

    def open(self, entry: HfsEntry) -> FileBase: ...


class RomfsFileEntry:
    name: str = ...
    parent: RomfsDirEntry = ...
    offset: int = ...
    size: int = ...


class RomfsDirEntry:
    name: str = ...
    parent: RomfsDirEntry = ...
    children: List[RomfsDirEntry] = ...
    files: List[RomfsFileEntry] = ...


class Romfs:
    def __init__(self, base: FileBase) -> None: ...

    def is_valid(self) -> bool: ...
    def parse(self) -> bool: ...

    def get_entries(self) -> Tuple[Dict[str, RomfsFileEntry], Dict[str, RomfsFileEntry]]: ...

    def open(self, RomfsFileEntry) -> FileBase: ...


class Nca:
    def __init__(self, base: FileBase) -> None: ...

    def is_valid(self) -> bool: ...
    def parse(self) -> bool: ...

    def get_distribution_type(self) -> int: ...
    def get_content_type(self) -> int: ...
    def get_size(self) -> int: ...
    def get_title_id(self) -> int: ...
    def get_sdk_ver(self) -> Tuple[int, int, int, int]: ...
    def get_rights_id(self) -> bytes: ...

    def get_sections(self) -> List[Union[Pfs, Romfs]]: ...
    def get_section_bounds(self) -> List[Tuple[int, int]]: ...


class Xci:
    def __init__(self, base: FileBase) -> None: ...

    def is_valid(self) -> bool: ...
    def parse(self) -> bool: ...

    def get_cart_type(self) -> int: ...

    def get_partitions(self) -> Dict[str, Hfs]: ...

def set_key(name: str, key: str) -> None: ...
def set_titlekey(name: str, key: str) -> None: ...
def set_user_titlekey(key: str) -> None: ...
def remove_user_titlekey() -> None: ...
def match(file: FileBase) -> int: ...

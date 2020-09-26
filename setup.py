#!/usr/bin/env python3

from glob import glob
from setuptools import setup, Extension

setup(
    name = "fnx",
    version = "1.0.0",
    packages = ["fnx"],
    package_dir = {"": "bindings"},
    ext_modules = [
        Extension("fnxbinds", ["bindings/bindings.cpp"] + glob("lib/*.cpp"),
            depends = glob("include/**/*.hpp", recursive=True),
            define_macros = [
                ("USE_GCRYPT", None),
            ],
            extra_compile_args = [
                "-std=gnu++20",
            ],
            include_dirs = ["include"],
            libraries    = ["gcrypt"]
        )
    ]
)

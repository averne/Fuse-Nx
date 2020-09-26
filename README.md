# Fuse-Nx

Mount Nintendo Switch archive as filesystems, and find/extract files from them.

<p float="left">
  <img src="https://user-images.githubusercontent.com/45773016/94306398-2e388780-ff73-11ea-8595-fc8425afd7a7.png" height="250" />
  <img src="https://user-images.githubusercontent.com/45773016/94306638-a69f4880-ff73-11ea-984d-3fc893990064.png" height="250" />
  <img src="https://user-images.githubusercontent.com/45773016/94306437-3db7d080-ff73-11ea-97f8-228ea32f3739.png" height="250" />
</p>

## Supported formats
- Nca (only AES-CTR encrypted/plaintext NCA3s are supported, BKTR encryption (for update NCAs) is unsupported)
- Xci
- Pfs
- Hfs
- Romfs

Formats are recognized based on magicnums/numeric contants present in their headers, and not their extensions.

## Usage

Refer to the built-in help (`-h`/`--help`). You can find help on a specific subcommand using `fuse-nx <subcmd> --help`, or `fuse-nx --help-all`.

### Windows

Windows is supported via WSL. To access the filesystem through the explorer, you will need to pass the `-o allow_other` flag to FUSE, and add `user_allow_other` to `/etc/fuse.conf`.

## Project layout

- [lib](lib): Library (fnx) for parsing the supported file formats;
- [src](src): Source code for the main application;
- [bindings](bindings): CPython (3) bindings for the library;
- [scripts](scripts): example Python scripts using these bindings.

## Building

### fuse-nx
This program depends on either libgcrypt or mbedtls for cryptographic operations. The former should be preferred when possible, as it makes uses of available hardware crypto extensions (whereas mbedtls only supports AES-NI).

The build process as follows:
```sh
meson build
meson compile -C build -j$(nproc)
```

If libcrypt is not found, it will fall back on the system installation of mbedtls, or failing that, on a clean build of it. mbedtls can be forcefully enabled by passing `-Dcryptobackend=mbedtls` on the configure step.

### Python bindings
```sh
python setup.py build
```

## Installing

### fuse-nx
```sh
sudo meson install -C build
```
For users of Arch-based distros, an [AUR package](https://aur.archlinux.org/packages/fuse-nx-git) is available.

### Python bindings
```sh
sudo python setup.py install
```
Or just `pip install -U git+https://github.com/averne/Fuse-Nx.git`.

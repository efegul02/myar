# myar

[![CI](https://github.com/efegul02/myar/actions/workflows/ci.yml/badge.svg)](https://github.com/efegul02/myar/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Valgrind](https://img.shields.io/badge/valgrind-0%20errors%2C%200%20leaks-brightgreen)

A small command-line tool written in C that packs many files into **one binary archive** and unpacks them again. Each file is stored with a header containing its **name**, **size** and a **CRC-32 checksum**.

> 🇹🇷 Türkçe açıklama için: [README.tr.md](README.tr.md)

## Demo

```text
$ make
cc -std=c11 -Wall -Wextra -Wpedantic -O2 -o myar src/main.c src/archive.c

$ ./myar pack backup.myar notes.txt photo.png data.csv
added: notes.txt (14 bytes)
added: photo.png (582113 bytes)
added: data.csv (12 bytes)

$ ./myar list backup.myar
        SIZE  CRC32     NAME
          14  01f43b66  notes.txt
      582113  10356ebc  photo.png
          12  bbbbb379  data.csv
3 file(s), 582139 bytes total

$ ./myar test backup.myar
OK      notes.txt
OK      photo.png
OK      data.csv

$ ./myar extract backup.myar restored/
extracted: restored/notes.txt (14 bytes)
extracted: restored/photo.png (582113 bytes)
extracted: restored/data.csv (12 bytes)
```

## Build & run

```sh
make              # builds ./myar
./myar --help
```

```sh
./myar pack    <archive> <file>...   # create an archive
./myar list    <archive>             # show the contents
./myar test    <archive>             # verify checksums only
./myar extract <archive> [dir]       # extract (never overwrites existing files)
```

Requirements: a C11 compiler and a POSIX system (Linux, macOS, WSL).

## Memory safety

**0 memory leaks detected by Valgrind** (valgrind 3.22.0, `--leak-check=full --track-origins=yes --track-fds=yes`).
Every command (`pack`, `list`, `test`, `extract`) and the error paths (duplicate names, truncated archive, invalid file, existing files) run with 0 errors, no leaked memory and no leaked file descriptors.

```sh
make valgrind     # needs: sudo apt install valgrind
make test         # functional tests
```

## How it works

### Architecture

```
src/main.c      command-line interface (argument parsing only)
src/archive.h   public API: ar_pack / ar_list / ar_test / ar_extract
src/archive.c   archive format, serialisation, CRC-32, file copying
tests/          functional tests + Valgrind script
docs/FORMAT.md  full format specification
```

### Archive format

```
[ Archive header: "MYAR" | version | file_count ]      12 bytes
[ Entry header: name[256] | size (u64) | crc32 ]       268 bytes
[ file data ... ]                                      size bytes
[ Entry header ] [ file data ] ...
```

The full specification is in [docs/FORMAT.md](docs/FORMAT.md).

### Algorithms and design decisions

- **Length-prefixed records:** every entry header stores the data size, so the reader jumps from entry to entry without scanning for separators. Binary data can contain any byte value.
- **CRC-32 (IEEE 802.3):** table-driven, one lookup per byte. The checksum is computed in a first pass over the input file because the header is written before the data.
- **Streaming copy:** files are copied in 64 KiB chunks, so memory usage stays constant regardless of file size. No `malloc` is needed anywhere.
- **Explicit little-endian serialisation:** fields are written byte by byte instead of `fwrite`-ing a struct, so the format does not depend on struct padding or the machine's endianness.
- **64-bit sizes:** files larger than 4 GB are supported.
- **Defensive reading:** magic number and version are checked, the entry size is compared with the remaining archive size, names containing `/`, `\`, `.` or `..` are rejected (zip-slip), and `fopen(..., "wbx")` refuses to overwrite existing files.
- **Atomic-ish packing:** all inputs are validated before the archive is created, and a failed `pack` removes the partial archive.

## Limitations / ideas

- Only regular files, no directories (names are stored without paths); names are limited to 255 bytes
- No compression
- Ideas: recursive directory support, variable-length names, extracting single files by name, zlib compression

## License

[MIT](LICENSE)

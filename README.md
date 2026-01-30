# cie-pseudocode-compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

Details can be found here: [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf)


## Dependencies

Linux
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  llvm-18-dev \
  llvm \
  clang \
  lld
```

---

mingw-w64 for Windows
```bash
pacman -Syu
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-llvm \
  mingw-w64-x86_64-clang \
  mingw-w64-x86_64-lld \
  mingw-w64-x86_64-make
```

## Build

Linux
```bash
mkdir build && cd build/
cmake ..
make
```

---

mingw-w64 for Windows
```bash
mkdir build && cd build/
export CMAKE_GENERATOR="MinGW Makefiles"
cmake ..
mingw32-make
```


# Note

DO NOT OPEN ANY ISSUES IF YOU COMPILE WITH VISUAL STUDIO OR ANYTHING EXCEPT MINGW-W64 (EVEN WITH MINGW-W64, WINDOWS IS LIKELY TO BE A PROBLEM MAKER)

ONLY TESTED ON DEBIAN (AND A BIT MINGW-W64)

Copyright (C) _ov4

This is free software; see the source for copying conditions.  There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

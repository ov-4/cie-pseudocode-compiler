# cie-pseudocode-compiler

A compiler for CIE Pseudocode (A Level 9618 Computer Science)

Details can be found here: [Pseudocode Guide for Teachers](https://www.cambridgeinternational.org/Images/697401-2026-pseudocode-guide-for-teachers.pdf)


## Dependencies

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


## Build

```bash
mkdir build && cd build/
cmake ..
make
```

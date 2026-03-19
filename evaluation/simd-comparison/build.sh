#!/usr/bin/env bash

cd ~/opt
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
export VERSION=21
sudo ./llvm.sh $VERSION all
rm llvm.sh

# install hyperfine
sudo apt install -y hyperfine

cd ~
git clone https://gitlab.arm.com/architecture/simd-loops
cd simd-loops

export C_COMPILER=clang-$VERSION
export OBJDUMP=llvm-objdump-$VERSION

make c-scalar scalar autovec-sve autovec-sve2 neon sve sve2 sve2p1 sve-intrinsics

hyperfine \
  --show-output \
  --export-csv result.csv \
  --warmup 1 \
  --runs 5 \
  -n 'c-scalar' './build/c-scalar/bin/simd_loops -k 1 -n 2' \
  -n 'neon' './build/neon/bin/simd_loops -k 1 -n 2'



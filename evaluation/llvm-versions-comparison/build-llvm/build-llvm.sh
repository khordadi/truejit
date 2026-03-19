#!/usr/bin/env bash

export ROOT=/tmp/llvm
export EVALUATION_DIR=/truejit/evaluation/llvm-versions-comparison
mkdir -p $ROOT

# get llvm-project
cd $ROOT
git clone https://github.com/llvm/llvm-project

# llvm versions: 9, 10, 11, 12, 13, 14, 15, 16, 17, 18
for VERSION in {9..18}; do
  cd $ROOT/llvm-project
  git clean -fdx && git reset --hard
  git checkout release/$VERSION.x
  # versions 9, 10, 11 need patching
  if [ $VERSION -lt 12 ]; then
    git apply $EVALUATION_DIR/build-llvm/llvm-project.patch
  fi
  export INSTALL_DIR=$ROOT/llvm-$VERSION
  mkdir -p $INSTALL_DIR
  mkdir -p build # build directory
  cd build
  cmake \
    -G Ninja \
    -DCMAKE_CXX_FLAGS="-pg" \
    -DCMAKE_C_FLAGS="-pg" \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_TARGETS_TO_BUILD=host \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
    ../llvm
  cmake --build . --target all -- -j$(nproc --all)
  cmake --install .
done

# get versions release dates
cd $ROOT/llvm-project
for VERSION in {9..18}; do
  git --no-pager show --no-patch --format=%ci origin/release/$VERSION.x | awk '{print $1}'
done

# get the lines of code for each version
cd $ROOT/llvm-project
for VERSION in {9..18}; do
  git checkout release/$VERSION.x
  # number of line only in llvm and clang directories
  cloc clang llvm
done

# build sqlite3 amalgamation
cd $ROOT
wget https://sqlite.org/2024/sqlite-amalgamation-3450300.zip
unzip sqlite-amalgamation-3450300.zip
cd sqlite-amalgamation-3450300
gcc -O3 shell.c sqlite3.c -lpthread -ldl -o sqlite3

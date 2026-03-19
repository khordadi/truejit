#!/usr/bin/env bash

#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# Installation of Necessary Packages
echo "Updating package data..."
sudo apt-get update -qq

echo "Installing required packages..."
sudo apt-get install -y autoconf automake build-essential cmake git-core libass-dev libfreetype6-dev \
  libgnutls28-dev libmp3lame-dev libsdl2-dev libtool libva-dev libvdpau-dev libvorbis-dev libxcb1-dev \
  libxcb-shm0-dev libxcb-xfixes0-dev meson ninja-build pkg-config texinfo wget yasm zlib1g-dev \
  libunistring-dev libaom-dev libdav1d-dev nasm

# Setup directories
echo "Creating directories..."
mkdir -p ~/ffmpeg_sources ~/bin

# NASM Installation or Update
echo "Installing NASM..."
cd ~/ffmpeg_sources
wget https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/nasm-2.16.01.tar.bz2
tar xjvf nasm-2.16.01.tar.bz2
cd nasm-2.16.01
./autogen.sh
./configure --prefix="$HOME/ffmpeg_build" --bindir="$HOME/bin"
make
make install

# libx264 Installation
echo "Installing libx264..."
cd ~/ffmpeg_sources
git -C x264 pull 2>/dev/null || git clone --depth 1 https://code.videolan.org/videolan/x264.git
cd x264
PKG_CONFIG_PATH="$HOME/ffmpeg_build/lib/pkgconfig" ./configure --prefix="$HOME/ffmpeg_build" \
  --bindir="$HOME/bin" --enable-static --enable-pic
make
make install

# libx265 Installation
echo "Installing libx265..."
sudo apt-get install -y libnuma-dev
cd ~/ffmpeg_sources
wget -O x265.tar.bz2 https://bitbucket.org/multicoreware/x265_git/get/master.tar.bz2
tar xjvf x265.tar.bz2
cd multicoreware*/build/linux
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="$HOME/ffmpeg_build" -DENABLE_SHARED=off ../../source
make
make install

# libvpx Installation
echo "Installing libvpx..."
cd ~/ffmpeg_sources
git -C libvpx pull 2>/dev/null || git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git
cd libvpx
./configure --prefix="$HOME/ffmpeg_build" --disable-examples --disable-unit-tests \
  --enable-vp9-highbitdepth --as=yasm
make
make install

# libfdk-aac Installation
echo "Installing libfdk-aac..."
cd ~/ffmpeg_sources
git -C fdk-aac pull 2>/dev/null || git clone --depth 1 https://github.com/mstorsjo/fdk-aac
cd fdk-aac
autoreconf -fiv
./configure --prefix="$HOME/ffmpeg_build" --disable-shared
make
make install

# FFmpeg Installation
echo "Installing FFmpeg..."
cd ~/ffmpeg_sources
wget -O benchmark-snapshot.tar.bz2 https://ffmpeg.org/releases/ffmpeg-snapshot.tar.bz2
tar xjvf benchmark-snapshot.tar.bz2
cd benchmark
PKG_CONFIG_PATH="$HOME/ffmpeg_build/lib/pkgconfig" ./configure \
  --prefix="$HOME/ffmpeg_build" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$HOME/ffmpeg_build/include" \
  --extra-ldflags="-L$HOME/ffmpeg_build/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --bindir="$HOME/bin" \
  --enable-gpl \
  --enable-gnutls \
  --enable-libaom \
  --enable-libass \
  --enable-libfdk-aac \
  --enable-libfreetype \
  --enable-libmp3lame \
  --enable-libopus \
  --enable-libsvtav1 \
  --enable-libdav1d \
  --enable-libvorbis \
  --enable-libvpx \
  --enable-libx264 \
  --enable-libx265 \
  --enable-nonfree
make
make install

echo "FFmpeg compilation and installation complete."
echo "Add $HOME/bin to your PATH if not already done so."

# Sourcing profiling to update path
source ~/.profile

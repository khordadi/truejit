#!/usr/bin/env bash

cd ~
git clone --recursive https://github.com/bytecodealliance/wasmtime
cd wasmtime
git checkout v23.0.0
cargo build --release
cargo run --release -- --version

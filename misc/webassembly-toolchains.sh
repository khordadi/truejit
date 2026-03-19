#!/usr/bin/env bash

# Install wasi-sdk
cd ~/opt
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/wasi-sdk_22.0_amd64.deb
sudo dpkg -i wasi-sdk_22.0_amd64.deb

# Install wasmer
curl https://get.wasmer.io -sSfL | sh
wasmer --version

# Install wasmtime
curl https://wasmtime.dev/install.sh -sSf | bash
wasmtime --version

# Install wavm
cd ~/opt
wget https://github.com/WAVM/WAVM/releases/download/nightly%2F2022-05-14/wavm-0.0.0-prerelease-linux.deb
sudo dpkg -i wavm-0.0.0-prerelease-linux.deb
wavm version

# Install wazero
cd /usr/local
curl https://wazero.io/install.sh | sudo sh
wazero version

# Install wasm3
cd ~/opt
wget https://github.com/wasm3/wasm3/releases/download/v0.5.0/wasm3-linux-x64.elf
sudo mv wasm3-linux-x64.elf /usr/local/bin/wasm3
sudo chmod +x /usr/local/bin/wasm3
wasm3 --version

# Install iwasm
cd ~/opt
wget https://github.com/bytecodealliance/wasm-micro-runtime/releases/download/WAMR-1.3.2/iwasm-1.3.2-x86_64-ubuntu-22.04.tar.gz
tar vfx iwasm-1.3.2-x86_64-ubuntu-22.04.tar.gz
sudo mv iwasm /usr/local/bin

# Install wasmedge
cd ~/opt
curl -sSf https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash
wasmedge --version

# Install wasmi
cargo install wasmi_cli
wasmi_cli --version

# Install graalwasm
cd ~/opt
wget https://gds.oracle.com/download/wasm/archive/graalwasm-24.0.0-linux-amd64.tar.gz
tar vfx graalwasm-24.0.0-linux-amd64.tar.gz
sudo cp -r ./graalwasm-24.0.0-linux-amd64/* /usr/local
sudo mv /usr/local/bin/wasm /usr/local/bin/graalwasm

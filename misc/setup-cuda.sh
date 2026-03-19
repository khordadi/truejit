#!/usr/bin/env bash

# check gpu existence
lspci | grep -i nvidia

# WARNING: The Nouveau kernel driver is currently in use by your system.  This driver is incompatible with the NVIDIA driver, and must be disabled before proceeding.
echo "blacklist nouveau" | sudo tee -a /etc/modprobe.d/blacklist-nouveau.conf
echo "options nouveau modeset=0" | sudo tee -a /etc/modprobe.d/blacklist-nouveau.conf
sudo update-initramfs -u
sudo reboot

# download and install nvidia driver: https://www.nvidia.com/Download/index.aspx?lang=en-us
wget https://us.download.nvidia.com/tesla/550.54.15/NVIDIA-Linux-x86_64-550.54.15.run
sudo sh NVIDIA-Linux-x86_64-550.54.15.run

# new
wget -O nvidia.deb https://us.download.nvidia.com/tesla/570.148.08/nvidia-driver-local-repo-ubuntu2404-570.148.08_1.0-1_amd64.deb
sudo dpkg -i nvidia.deb

# reboot the system
sudo reboot

# check if NVIDIA GPU is available
nvidia-smi

# install gpu monitoring tools
sudo apt update && sudo apt install -y nvtop

# download and install cuda: https://developer.nvidia.com/cuda-downloads
wget https://developer.download.nvidia.com/compute/cuda/12.4.1/local_installers/cuda_12.4.1_550.54.15_linux.run
sudo sh cuda_12.4.1_550.54.15_linux.run
# add cuda to path
export PATH=/usr/local/cuda-12.4/bin${PATH:+:${PATH}}
export LD_LIBRARY_PATH=/usr/local/cuda-12.4/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}
echo "export PATH=/usr/local/cuda-12.4/bin${PATH:+:${PATH}}" >>~/.zshrc
echo "export LD_LIBRARY_PATH=/usr/local/cuda-12.4/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" >>~/.zshrc
source ~/.zshrc

# hello world cuda program
cat <<EOF >main.cu
#include <iostream>
#include <cuda_runtime.h>

__global__ void helloFromGPU()
{
    printf("Hello from GPU!\n");
}

int main()
{
    helloFromGPU<<<1, 10>>>();
    cudaDeviceReset();
    return 0;
}
EOF

nvcc -o main main.cu -O3
./main

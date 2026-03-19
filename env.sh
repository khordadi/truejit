#!/usr/bin/env bash

# unmount /mydata
sudo umount /mydata
sudo rm -rf /mydata /tmp
export DISK=/dev/mapper/emulab-node0--bs
sudo mkdir /tmp
sudo mount $DISK /tmp
sudo chown -R $(whoami) /tmp
sudo chmod 777 /tmp
df -h /tmp

sudo locale-gen en_GB.UTF-8
sudo update-locale LANG=en_GB.UTF-8

sudo apt update && sudo apt install -y \
  vim wget git tmux zsh htop curl ccache ninja-build tree bash less autoconf zip unzip \
  build-essential \
  file \
  ripgrep bat fd-find \
  shfmt \
  rsync \
  hexyl \
  python2.7 python2.7-dev \
  python3 python3-dev python3-pip python3-setuptools python3-venv \
  locales \
  libncursesw5-dev \
  ranger caca-utils highlight atool w3m poppler-utils mediainfo \
  openssh-server \
  libssl-dev \
  gdb \
  iproute2 iputils-ping \
  telnet \
  gnupg2 \
  sudo \
  man manpages-posix \
  ncdu duf \
  lsb-release wget software-properties-common gnupg \
  cloc \
  time \
  graphviz \
  glpk-utils


sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

git clone --depth=1 https://github.com/romkatv/powerlevel10k.git ${ZSH_CUSTOM:-$HOME/.oh-my-zsh/custom}/themes/powerlevel10k
git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting
git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions
git clone https://github.com/zsh-users/zsh-completions ${ZSH_CUSTOM:=~/.oh-my-zsh/custom}/plugins/zsh-completions

tee ~/.zshrc <<EOF
export PATH=$HOME/bin:/usr/local/bin:$PATH
export ZSH="$HOME/.oh-my-zsh"
ZSH_THEME="powerlevel10k/powerlevel10k"
plugins=(git docker docker-compose zsh-syntax-highlighting zsh-autosuggestions)
fpath+=${ZSH_CUSTOM:-${ZSH:-~/.oh-my-zsh}/custom}/plugins/zsh-completions/src
export LC_ALL=en_GB.UTF-8
export LANG=en_GB.UTF-8
export LANGUAGE=en_GB.UTF-8
export BAT_PAGER="less -RF"
export EDITOR=nano
alias htop="sudo htop"
alias bat="batcat"
alias fd="fdfind"
alias ncdu="ncdu --color=dark"
alias joop="jupyter lab --no-browser --port=8888 --ip=127.0.0.1"
export PATH=$HOME/.local/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
source $HOME/.cargo/env
source $HOME/.oh-my-zsh/oh-my-zsh.sh
EOF

# install rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

# setup ranger
cd ~
ranger
ranger --copy-config=all

# tmux
cd ~
git clone https://github.com/gpakosz/.tmux.git
ln -s -f .tmux/.tmux.conf
cp .tmux/.tmux.conf.local .

# install nano
sudo apt remove -y nano
export VERSION=8.2
export MAJOR_VERSION=$(echo $VERSION | cut -d. -f1)
cd /tmp
wget -O nano.tar.gz https://www.nano-editor.org/dist/v"${MAJOR_VERSION}"/nano-"${VERSION}".tar.gz
tar vfx nano.tar.gz
rm nano.tar.gz
mv nano* nano
cd nano
./configure
make -j$(nproc -a)
sudo make install
curl https://raw.githubusercontent.com/scopatz/nanorc/master/install.sh | sh
cd ..
sudo rm -rf nano

# install docker
sudo apt update && sudo apt install -y apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list >/dev/null
sudo apt update && sudo apt install -y docker-ce
sudo usermod -aG docker ${USER}

# install cmake
mkdir -p ~/opt
cd ~/opt
export VERSION=3.30.2
wget -O cmake.sh https://github.com/Kitware/CMake/releases/download/v$VERSION/cmake-$VERSION-linux-"$(uname -m)".sh
sudo chmod +x cmake.sh
sudo ./cmake.sh --skip-license --prefix=/usr/local
rm cmake.sh
cd

# install conda
mkdir -p ~/opt
cd ~/opt
wget -O miniconda.sh https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-"$(uname -m)".sh
bash miniconda.sh -b -p $HOME/opt/miniconda
$HOME/opt/miniconda/bin/conda init zsh
rm miniconda.sh
cd


# activate the env first!
pip install --upgrade pip

# install jupyter and other python packages
pip install jupyter matplotlib numpy pandas scikit-learn scipy bokeh seaborn pulp

# *** GPU and CUDA setup **********************
# check if NVIDIA GPU is available
lspci | grep -i nvidia
# install ubuntu-drivers and setup NVIDIA GPU use ubuntu-drivers
sudo apt update && sudo apt install -y ubuntu-drivers-common && sudo ubuntu-drivers autoinstall
# reboot the system
# check if NVIDIA GPU is available
nvidia-smi
# install gpu monitoring tools
pip install gpustat nvitop
sudo apt install -y nvtop
# *** setup nvidia gpu and cuda before installing pytorch ***

# install pytorch with gpu support (only with GPU)
pip install torch torchvision torchaudio
# **********************************************

# install latex
sudo apt install -y cm-super dvipng texlive-latex-extra texlive-latex-recommended texlive-fonts-extra latexmk


# ============================
# install truejit dependencies
# ============================
# install LLVM 17
cd ~/opt
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
export VERSION=17
sudo ./llvm.sh $VERSION all
rm llvm.sh
for i in /usr/bin/clang /usr/bin/clang++ /usr/bin/clang-cpp /usr/bin/clang-format /usr/bin/clang-tidy /usr/bin/clangd /usr/bin/llvm-dis /usr/bin/llvm-link ; do
  sudo ln -s $i-$VERSION $i
done

# build nlohmann/json
cd ~/opt
git clone https://github.com/nlohmann/json
cd json
git checkout v3.11.3
mkdir build
cd build
cmake -GNinja -DJSON_BuildTests=OFF -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j$(nproc --all)
sudo cmake --build . --target install -- -j$(nproc --all)
rm -rf ~/opt/json

# install fmt
cd ~/opt
git clone https://github.com/fmtlib/fmt
cd fmt
git checkout 10.1.1
mkdir build
cd build
cmake -GNinja \
  -DFMT_TEST=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DFMT_DOC=OFF \
  -DCMAKE_CXX_VISIBILITY_PRESET=hidden \
  -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON \
  -DFMT_PEDANTIC=ON \
  -DFMT_WERROR=ON \
  ..
cmake --build . --config Release -- -j$(nproc --all)
sudo cmake --build . --target install -- -j$(nproc --all)
rm -rf ~/opt/fmt

# install WABT
cd ~/opt
git clone --recursive https://github.com/WebAssembly/wabt
cd wabt
git submodule update --init
mkdir build
cd build
cmake -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -- -j$(nproc --all)
sudo cmake --install .
rm -rf ~/opt/wabt

# build uvwasi
cd ~/opt
sudo apt-get update && sudo apt-get install -y libuv1-dev
git clone https://github.com/nodejs/uvwasi
cd uvwasi
mkdir build
cd build
cmake -GNinja -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE -DUVWASI_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j$(nproc --all)
sudo cmake --build . --target install -- -j$(nproc --all)
rm -rf ~/opt/uvwasi

# build boost 1.84 and run ldconfig
cd ~/opt
wget https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
tar -xzf boost_1_84_0.tar.gz
cd boost_1_84_0
./bootstrap.sh --with-libraries=filesystem,system,thread,program_options,regex,iostreams,log,locale,chrono,timer,date_time,atomic,coroutine,context,stacktrace,serialization
sudo ./b2 -j$(nproc --all) release install
sudo rm -rf ~/opt/boost_1_84_0.tar.gz ~/opt/boost_1_84_0
cd
sudo ldconfig

# install pytorch
cd ~/opt
wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.1.0%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-2.1.0+cpu.zip
sudo cp -r libtorch/* /usr/local/
rm -rf libtorch-cxx11-abi-shared-with-deps-2.1.0+cpu.zip libtorch
pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu

# install grpc (put after 'pytorch' to avoid link errors)
cd ~/opt
sudo apt-get update && sudo apt-get install -y build-essential autoconf libtool pkg-config
rm -rf grpc
git clone --recurse-submodules -b v1.58.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
pushd grpc
mkdir -p cmake/build
pushd cmake/build
cmake \
  -GNinja \
  -DgRPC_INSTALL=ON \
  -DABSL_PROPAGATE_CXX_STD=ON \
  -DgRPC_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  ../..
cmake --build . -- -j$(nproc --all)
sudo cmake --build . --target install -- -j$(nproc --all)
popd
popd
rm -rf grpc



cargo install twiggy

# Install wasi-sdk
cd ~/opt
if [ "$(uname -m)" = "aarch64" ]; then
  wget -O wasi-sdk.deb https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-arm64-linux.deb
fi
if [ "$(uname -m)" = "x86_64" ]; then
  wget -O wasi-sdk.deb https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/wasi-sdk_22.0_amd64.deb
fi
sudo dpkg -i wasi-sdk.deb
rm wasi-sdk.deb

# Install wasmtime
curl https://wasmtime.dev/install.sh -sSf | bash
$HOME/.wasmtime/bin/wasmtime --version

# Install wasmer
curl https://get.wasmer.io -sSfL | sh
$HOME/.wasmer/bin/wasmer --version



# kubernetes
# install kubectl
cd ~/opt
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
sudo install -o root -g root -m 0755 kubectl /usr/local/bin/kubectl
rm kubectl
popd
kubectl version --client

# install go
cd ~/opt
wget https://go.dev/dl/go1.24.4.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.24.4.linux-amd64.tar.gz
rm go1.24.4.linux-amd64.tar.gz
popd
echo 'export PATH=$PATH:/usr/local/go/bin' >>~/.zshrc
omz reload


# build k9s from source
cd ~/opt
git clone --recurse https://github.com/derailed/k9s
cd k9s
make build -j$(nproc --all)
sudo mv ./execs/k9s /usr/local/bin/k9s
cd ..
rm -rf k9s
echo 'export TERM=xterm-256color' >>~/.zshrc
echo 'export KUBE_EDITOR=nano' >>~/.zshrc
omz reload

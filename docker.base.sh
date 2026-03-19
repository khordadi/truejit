#!/usr/bin/env bash

# allocate a new machine with enough storage space to build the image, and install docker on it.
sudo apt update && sudo apt install -y apt-transport-https ca-certificates curl software-properties-common
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list >/dev/null
sudo apt update && sudo apt install -y docker-ce
sudo usermod -aG docker ${USER}


docker rm -f truejit
docker run \
  -it \
  --name=truejit \
  --hostname=truejit \
  --platform=linux/amd64 \
  -p 10000:22 \
  ubuntu:22.04

apt update && apt install -y sudo locales zsh openssh-server

# create a new user
export USER_NAME=khordadi
export PASSWORD=amir
useradd -m "$USER_NAME" -s /bin/zsh
echo "$USER_NAME:$PASSWORD" | chpasswd
adduser "$USER_NAME" sudo
echo "$USER_NAME ALL=(ALL) NOPASSWD: ALL" >>/etc/sudoers.d/"$USER_NAME"
su - "$USER_NAME"

# exit and run from host
docker start truejit && docker exec truejit sudo service ssh restart
ssh -p 10000 localhost

# ===============================
# use env.sh
# ===============================
sudo apt update && sudo apt install -y \
  vim wget git tmux zsh curl ninja-build bash less autoconf zip unzip \
  build-essential \
  file \
  python2.7 python2.7-dev \
  python3 python3-dev python3-pip python3-setuptools python3-venv \
  locales \
  libssl-dev \
  gdb \
  lsb-release wget software-properties-common gnupg \
  time

# remove all the cacehed and /tmp directories
sudo rm -rf \
  /var/lib/apt/lists/* \
  /var/cache/apt/archives/* \
  /var/tmp/* \
  /tmp/*

# clear history
history -c

# exit and commit the changes
docker stop truejit

docker login --username khordadi

docker commit truejit khordadi/truejit:base
docker push khordadi/truejit:base

#!/usr/bin/env bash

cd /tmp/truejit
docker pull khordadi/truejit:base
docker build -t khordadi/truejit:latest -f Dockerfile .
docker rmi $(docker images -f "dangling=true" -q) > /dev/null 2>&1 || true
docker push khordadi/truejit:latest
cd -

#!/usr/bin/env bash

cd /tmp/truejit
docker pull khordadi/truejit:base
docker build -t khordadi/truejit:latest -f Dockerfile .
docker build -t khordadi/truejit:worker -f Dockerfile.worker .
docker rmi $(docker images -f "dangling=true" -q) > /dev/null 2>&1 || true
docker push khordadi/truejit:latest
cd -



cd /tmp/truejit/
docker compose up --scale workers=10 -d
docker compose down
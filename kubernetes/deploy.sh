#!/usr/bin/env bash

# from laptop (macos)
scp ~/.ssh/id_rsa khordadi@truejit:/users/khordadi/.ssh

pip install pyyaml paramiko

# login
docker login --username khordadi

# configure kubectl
/tmp/truejit/kubernetes/config.py xxx

kubectl apply -f /tmp/truejit/kubernetes/cluster.yaml
kubectl delete -f /tmp/truejit/kubernetes/cluster.yaml

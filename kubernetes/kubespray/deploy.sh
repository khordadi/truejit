#!/usr/bin/env bash

export CONFIG_DIR=$(pwd)

# Extract IP addresses from the XML file
IP_ADDRESSES=$(xmllint --xpath '//*[local-name()="ip"]/@address' $CONFIG_DIR/manifest.xml | awk -F '"' '{for (i=2; i<=NF; i+=2) print $i}')
ACCESS_IP_ADDRESSES=$(xmllint --xpath '//*[local-name()="host"]/@ipv4' $CONFIG_DIR/manifest.xml | awk -F '"' '{for (i=2; i<=NF; i+=2) print $i}')

# Convert the extracted IP addresses into bash arrays
declare -a IPS=($IP_ADDRESSES)
# print the IP addresses
for i in "${!IPS[@]}"; do
  echo "IP[$i]: ${IPS[$i]}"
done

declare -a ACCESS_IPS=($ACCESS_IP_ADDRESSES)
# print the IP addresses
for i in "${!ACCESS_IPS[@]}"; do
  echo "ACCESS_IP[$i]: ${ACCESS_IPS[$i]}"
done

# copy both the public key and private key to the remote hosts
#for i in "${!ACCESS_IPS[@]}"; do
#  scp -o StrictHostKeyChecking=no -i ~/.ssh/id_rsa -r ~/.ssh/id_rsa* khordadi@${ACCESS_IPS[$i]}:~/.ssh &
#done
#wait

# Construct the configuration string
CONFIG_STRING=""
for i in "${!ACCESS_IPS[@]}"; do
  #  CONFIG_STRING+="node$i,${IPS[$i]},${ACCESS_IPS[$i]} "
  CONFIG_STRING+="${ACCESS_IPS[$i]} "
done
export CONFIG_STRING

echo $CONFIG_STRING

rm -rf /tmp/kubespray
git clone https://github.com/kubernetes-sigs/kubespray /tmp/kubespray
cd /tmp/kubespray || exit
git checkout release-2.25
git apply $CONFIG_DIR/kubespray.patch

python3 -m venv venv
source venv/bin/activate
echo "paramiko" >>requirements.txt
echo "ruamel.yaml" >>requirements.txt
pip install -U -r requirements.txt

cp -rfp inventory/sample inventory/truejit-cluster

# if macos, export OBJC_DISABLE_INITIALIZE_FORK_SAFETY=YES
if [[ "$OSTYPE" == "darwin"* ]]; then
  export OBJC_DISABLE_INITIALIZE_FORK_SAFETY=YES
fi

export DEBUG=true
export CONFIG_FILE=inventory/truejit-cluster/hosts.yaml
export KUBE_CONTROL_HOSTS=1
python3 contrib/inventory_builder/inventory.py $CONFIG_STRING

# workaround for ```FAILED! => {"msg": "Unable to execute ssh command line on a controller due to: [Errno 24] Too many open files"}```
# https://github.com/mitogen-hq/mitogen/issues/549#issuecomment-466086953
ulimit -n 10000

ansible-playbook -i inventory/truejit-cluster/hosts.yaml --become --become-user=root reset.yml
ansible-playbook -i inventory/truejit-cluster/hosts.yaml --become --become-user=root cluster.yml

cd $CONFIG_DIR
./config.py ${ACCESS_IPS[0]}

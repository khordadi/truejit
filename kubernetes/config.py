#!/usr/bin/env python

import sys
import os
import paramiko
import yaml


def main():
    host = sys.argv[1]
    username = "khordadi"
    private_key = "/users/khordadi/.ssh/id_rsa"

    client = paramiko.client.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=username, key_filename=private_key)
    stdin, stdout, stderr = client.exec_command("sudo cat /root/.kube/config")

    yaml_file = yaml.safe_load(stdout.read().decode())
    assert yaml_file is not None

    with open("/tmp/truejit/kubernetes/template.kubeconfig") as stream:
        try:
            template = yaml.safe_load(stream)
        except yaml.YAMLError as err:
            print(err)
    client.close()

    server = host
    certificate_authority_data = yaml_file["clusters"][0]["cluster"]["certificate-authority-data"]
    client_certificate_data = yaml_file["users"][0]["user"]["client-certificate-data"]
    client_key_data = yaml_file["users"][0]["user"]["client-key-data"]

    template["clusters"][0]["cluster"]["certificate-authority-data"] = certificate_authority_data
    template["clusters"][0]["cluster"]["server"] = f'https://{server}:6443'
    template["users"][0]["user"]["client-certificate-data"] = client_certificate_data
    template["users"][0]["user"]["client-key-data"] = client_key_data

    # mkdir /users/khordadi/.kube
    if not os.path.exists("/users/khordadi/.kube"):
        os.makedirs("/users/khordadi/.kube")

    with open("/users/khordadi/.kube/config", "w") as f:
        yaml.dump(template, f, default_flow_style=False)

    print("Kubeconfig file has been created successfully!")


if __name__ == '__main__':
    main()

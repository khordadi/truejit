#!/usr/bin/env bash

# Generate a private key .pem
openssl genpkey -algorithm RSA -out private_key.pem
# Extract the public key from the private key
openssl rsa -pubout -in private_key.pem -out public_key.pem

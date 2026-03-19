#!/usr/bin/env bash

# validation
if [ -z "$1" ]; then
  echo "Usage: $0 <orchestrator-ip>"
  exit 1
fi

while true; do
  worker --orchestrator-ip=$1
done

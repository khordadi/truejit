#!/usr/bin/env bash

if [ -z "$1" ]; then
  echo "Usage: $0 <number_of_workers>"
  exit 1
fi

for i in $(seq 1 "$1"); do
  export PORT=$((60000 + i))
#  echo "/tmp/truejit/debug/tools/worker --port=$PORT"
#  /tmp/truejit/debug/tools/worker --orchestrator-ip=0.0.0.0 --port="$PORT" &
  echo "/tmp/truejit/release/tools/worker --port=$PORT"
  /tmp/truejit/release/tools/worker --orchestrator-ip=0.0.0.0 --port="$PORT" &
done
wait

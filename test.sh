#!/bin/bash

url="http://localhost:8080"

# Send 5 requests rapidly
for i in $(seq 1 10); do
    curl -s -w "%{http_code}\n" -o /dev/null $url
    sleep 0.12
done

# Wait for some time to reset the EMA
sleep 5

# Send another 5 requests rapidly
for i in $(seq 1 5); do
    curl -s -w "%{http_code}\n" -o /dev/null $url
    sleep 0.11
done

#!/bin/bash

url="http://localhost:8080"

send_request() {
    local response
    response=$(curl -s -w "%{http_code}" -o /dev/null $url)
    echo "Response: $response"
}

for i in $(seq 1 15); do
    send_request
done

sleep 2

for i in $(seq 1 5); do
    send_request
    sleep 0.12
done

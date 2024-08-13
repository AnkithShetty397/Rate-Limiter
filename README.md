# Rate Limiter Middleware

## Overview

This project implements a basic rate limiting middleware in C++. The middleware tracks incoming requests based on IP addresses and enforces a request threshold to prevent abuse. It uses multithreading to handle request counting and periodic decrementing.

## Features

- **Rate Limiting:** Limits the number of requests from a single IP address.
- **Thread Safety:** Uses mutexes to ensure thread-safe access to shared resources.
- **Periodic Decrementing:** Decreases request counts at regular intervals.
- **Request Processing:** Handles incoming requests and decides whether to process or reject them based on the rate limit.



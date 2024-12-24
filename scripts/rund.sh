#!/usr/bin/env bash
docker build . -t ltest:latest 
docker run -d -it -v "$(pwd)":/Ltest:z ltest

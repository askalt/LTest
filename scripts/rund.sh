#!/usr/bin/env bash
docker build . -t ltest:latest 
docker run -it --mount type=bind,source="$(pwd)",target=/LTest ltest 

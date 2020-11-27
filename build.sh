#!/bin/bash

INCLUDE_DIRS="-I$(pwd)/../Video_Codec_SDK_11.0.10/Interface -I/usr/local/cuda/include"
LIB_DIRS="/usr/local/cuda/lib64"

## nvenc 
gcc -g -O0 -fPIC ${INCLUDE_DIRS} nvenc.c -lnvidia-encode -lcuda -o nvenc

## cuda devices info
# g++ -g -O0 -fPIC ${INCLUDE_DIRS} -L${LIB_DIRS} cudadevices.cpp -lcuda -lcudart -o cudadevices

## cuda add
nvcc cuda_add.cu

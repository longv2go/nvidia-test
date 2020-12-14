PWD := $(shell pwd)
INCLUDE_DIRS := -I$(PWD)/../Video_Codec_SDK_11.0.10/Interface -I/usr/local/cuda/include
LIB_DIRS := "/usr/local/cuda/lib64"

all: nvenc opencl_test cuda_add

nvenc: nvenc.c
	gcc -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) nvenc.c -lnvidia-encode -lcuda -o nvenc
	
opencl_test: opencl_test.c
	gcc -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) opencl_test.c -lOpenCL -o opencl_test

cuda_add: cuda_add.cu
	nvcc cuda_add.cu --output-file cuda_add

clean:
	rm cuda_add nvenc opencl_test *.o

.PHONY: clean
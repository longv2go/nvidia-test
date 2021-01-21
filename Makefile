PWD := $(shell pwd)
INCLUDE_DIRS := -I$(PWD)/Interface -I/usr/local/cuda/include -I/usr/local/cuda-11.1/targets/x86_64-linux/include
LIB_DIRS := "/usr/local/cuda/lib64"

all: nvenc nvdec opencl_test cuda_add nvml_info

nvenc: nvenc.c
	gcc -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) nvenc.c -lnvidia-encode -lcuda -o nvenc
	
opencl_test: opencl_test.c
	gcc -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) opencl_test.c -lOpenCL -o opencl_test

cuda_add: cuda_add.cu
	nvcc cuda_add.cu --output-file cuda_add

nvdec: nvdec.cpp
	g++ -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) nvdec.cpp -lnvcuvid -lcuda -lavformat -lavcodec -lavutil -o nvdec

nvml_info: nvml_info.cpp
	g++ -g -O0 $(INCLUDE_DIRS) -L$(LIB_DIRS) nvml_info.cpp -lnvidia-ml -o nvml_info

clean:
	rm cuda_add nvdec nvenc opencl_test nvml_info *.o

.PHONY: clean

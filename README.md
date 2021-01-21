# nvidia-test

Nvidia Codec SDK
https://docs.nvidia.com/video-technologies/video-codec-sdk/index.html

CUDA Toolkit
https://developer.nvidia.com/zh-cn/cuda-downloads
https://docs.nvidia.com/cuda/index.html


# 程序

* nvenc.c 使用 nvidia 显卡编码 h264
* cuda_add.c 使用 cuda 实现了向量相加
* utils/FFmpeg* 来自 nvidia 的 demo 程序
* nvdec.cpp 使用 nvidia 显卡硬解码 h264
* nvml_info.cpp 使用 NVML 接口获取 GPU 信息

# 编译

`make`

## 依赖

* Interface 头文件目录来自 Nvidia Codec SDK
* 安装 libnvidia-encode libnvidia-decode

在 Device: Tesla T4, Driver Ver: 455.38, CUDA Ver: 11.1 环境下可编译

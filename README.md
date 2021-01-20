# nvidia-test

Nvidia Codec SDK
https://docs.nvidia.com/video-technologies/video-codec-sdk/index.html

CUDA Toolkit
https://developer.nvidia.com/zh-cn/cuda-downloads
https://docs.nvidia.com/cuda/index.html


# 文件

* nvenc.c 使用 nvidia 显卡编码 h264
* cuda_add.c 使用 cuda 实现了向量相加
* utils/FFmpeg* 来自 nvidia 的 demo 程序
* nvdec.cpp 使用 nvidia 显卡硬解码 h264
* nvml_info.cpp 使用 NVML 接口获取 GPU 信息

# 编译

`make`

## 依赖

* 首先下载 https://developer.nvidia.com/nvidia-video-codec-sdk/download, 然后相应的修改 Makefile 的 INCLUDE_DIRS 变量
* 安装 libnvidia-encode libnvidia-decode

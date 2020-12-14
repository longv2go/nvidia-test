#include "stdio.h"

#include "utils/FFmpegDemuxer.h"
#include "utils/FFmpegStreamer.h"
#include "nvcuvid.h"

#define CK(res) if (res != CUDA_SUCCESS) { printf("line:%d, result error: %d\n", __LINE__, res); abort(); }

#define INPUT_FILE "av_test_in.h264"
#define OUTPUT_FILE "av_test_out.yuv"
#define V_WIDTH 1280
#define V_HEIGHT 720

static CUcontext _cuctx = NULL;
static CUvideoparser _parser = NULL;
static CUvideodecoder _decoder = NULL;
static FFmpegDemuxer *_demuxer = NULL;
static FFmpegStreamer *_out_stream = NULL;
static CUstream _stream = NULL;

#define P_CAP(caps, key) printf(#key ": %d\n", caps.key)
void print_dec_caps(CUVIDDECODECAPS caps) {
    printf("DEC CAPS:\n");
    P_CAP(caps, bIsSupported);
    P_CAP(caps, nNumNVDECs);
    P_CAP(caps, nMaxWidth);
    P_CAP(caps, nMaxHeight);
    P_CAP(caps, nMinWidth);
    P_CAP(caps, nMinHeight);
    P_CAP(caps, bIsHistogramSupported);
    P_CAP(caps, nCounterBitDepth);
    P_CAP(caps, nMaxHistogramBins);
    printf("\n");
}

void query_caps() {
    CUVIDDECODECAPS caps = {};
    caps.eCodecType = cudaVideoCodec_H264;
    caps.eChromaFormat = cudaVideoChromaFormat_420;
    caps.nBitDepthMinus8 = 0;
    CUresult result = cuvidGetDecoderCaps(&caps);
    CK(result);

    print_dec_caps(caps);
}

#define P_FMT_D(format, key) printf(#key ": %d\n", format->key)
void print_video_format(CUVIDEOFORMAT *format) {
    printf("\nVIDEO FORMAT:\n");
    P_FMT_D(format, codec);
    P_FMT_D(format, progressive_sequence);
    P_FMT_D(format, min_num_decode_surfaces);
    P_FMT_D(format, coded_width);
    P_FMT_D(format, coded_height);
    P_FMT_D(format, chroma_format);
    P_FMT_D(format, bit_depth_luma_minus8);
    printf("\n");
}

// parser callbacks, sps, pps 有变更，需要重新创建 decoder
int handleSequence(void *ud, CUVIDEOFORMAT *format) {
    print_video_format(format);

    // decoder
    CUVIDDECODECREATEINFO pdci = {0};
    pdci.CodecType = format->codec;
    pdci.ulWidth = format->coded_width;
    pdci.ulHeight = format->coded_height;
    pdci.ulNumDecodeSurfaces = format->min_num_decode_surfaces;
    pdci.ulTargetWidth = format->coded_width;;
    pdci.ulTargetHeight = format->coded_height;
    pdci.ChromaFormat = format->chroma_format;
    pdci.OutputFormat = cudaVideoSurfaceFormat_NV12;
    pdci.bitDepthMinus8 = format->bit_depth_luma_minus8;
    pdci.ulNumOutputSurfaces = 2;

    if (format->progressive_sequence)
        pdci.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    else
        pdci.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;

    CUresult result = cuvidCreateDecoder(&_decoder, &pdci);
    CK(result);

    // 要正确的返回，否则或导致在 cuvidParseVideoData 奇怪的错误
    return format->min_num_decode_surfaces;
}

#define P_PIC(param, key) printf(#key ": %d\n", param->key)
void print_pic_param(CUVIDPICPARAMS *param) {
    P_PIC(param, PicWidthInMbs);
    P_PIC(param, FrameHeightInMbs);
    P_PIC(param, CurrPicIdx);
    P_PIC(param, nBitstreamDataLen);
    P_PIC(param, nNumSlices);
}

// 解码
int handleDecodePicture(void *ud, CUVIDPICPARAMS * param) {
    // print_pic_param(param);
    CUresult result = cuvidDecodePicture(_decoder, param);
    CK(result);
    return 1;
}

// 参考 nvidia 示例代码
int GetWidth() {
    return (V_WIDTH + 1) & ~1;
}

int GetFrameSize() {
   return GetWidth() * (V_HEIGHT + (V_HEIGHT * 0.5 * 1)) * 1;
}

// 获取解码数据
int handleDisplayPicture(void *ud, CUVIDPARSERDISPINFO *pDispInfo) {
    static int nFrame = 0;

    printf("pix index: %d\n", pDispInfo->picture_index);
    printf("top_field_first: %d\n", pDispInfo->top_field_first);
    printf("second_field: %d\n", pDispInfo->repeat_first_field);

    CUVIDPROCPARAMS videoProcessingParameters = {0};
    videoProcessingParameters.progressive_frame = pDispInfo->progressive_frame;
    // videoProcessingParameters.second_field = pDispInfo->repeat_first_field + 1;
    // videoProcessingParameters.top_field_first = pDispInfo->top_field_first;
    // videoProcessingParameters.unpaired_field = pDispInfo->repeat_first_field < 0;
    videoProcessingParameters.output_stream = _stream;

    CUdeviceptr dpSrcFrame = 0;
    unsigned int nSrcPitch = 0;

    // 如果 decoder 创建的参数有问题，报错可能会出现在这里
    CUresult result = cuvidMapVideoFrame(_decoder, pDispInfo->picture_index, &dpSrcFrame, &nSrcPitch, &videoProcessingParameters);
    CK(result);
    printf("pitch: %d\n", nSrcPitch);

    CUVIDGETDECODESTATUS DecodeStatus = {};
    result = cuvidGetDecodeStatus(_decoder, pDispInfo->picture_index, &DecodeStatus);
    CK(result);
    printf("decode status: %d\n", DecodeStatus.decodeStatus);

    // 获取解码
    // int frameSize = (ChromaFormat == cudaVideoChromaFormat_444) ? nPitch * (3*nheight) : nPitch * (nheight + (nheight + 1) / 2);
    int framesize = V_HEIGHT * V_WIDTH * 3 / 2; 
    framesize = GetFrameSize();
    printf("frame size: %d\n", framesize);

    char *hostptr = NULL;
    result = cuMemAllocHost((void**)&hostptr, framesize);
    CK(result);

    if (hostptr) {
        // 这里注意，由于显卡的字节对齐方式是不同的，所以不能直接 copy 出来到内存 https://www.jianshu.com/p/eace8c08b169
        // 代码参考 nvidia 的 sample

        // copy luma(亮度数据)
        CUDA_MEMCPY2D m = { 0 };
        m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        m.srcDevice = dpSrcFrame;
        m.srcPitch = nSrcPitch;
        m.dstMemoryType = CU_MEMORYTYPE_HOST;
        m.dstHost = hostptr;
        m.dstDevice = (CUdeviceptr)hostptr;
        m.dstPitch = GetWidth();
        m.WidthInBytes = GetWidth();
        m.Height = V_HEIGHT;
        result = cuMemcpy2DAsync(&m, _stream);
        CK(result);

        // copy chroma(色度数据)
        m.srcDevice = (CUdeviceptr)((uint8_t *)dpSrcFrame + m.srcPitch * ((V_HEIGHT + 1) & ~1));
        m.dstDevice = (CUdeviceptr)(m.dstHost = hostptr + m.dstPitch * V_HEIGHT);
        m.Height = V_HEIGHT / 2;
        result = cuMemcpy2DAsync(&m, _stream);
        CK(result);
    }

    result = cuvidUnmapVideoFrame(_decoder, dpSrcFrame);
    CK(result);

    // 保存到磁盘
    printf("Get decoded frame: %d\n", nFrame);
    _out_stream->Stream((uint8_t *)hostptr, framesize, nFrame++);

    if (hostptr) {
        cuMemFreeHost(hostptr);
    }

    return 1;
}

int main() {
    // context
    CUresult result = cuInit(0);
    CK(result);

    CUdevice cu_device;
    result = cuDeviceGet(&cu_device, 0);
    CK(result);

    result = cuCtxCreate(&_cuctx, 0, cu_device);
    CK(result);

    query_caps();

    // parser

    CUVIDPARSERPARAMS params;
    params.CodecType = cudaVideoCodec_H264;
    params.ulMaxNumDecodeSurfaces = 1;
    params.pfnSequenceCallback = handleSequence;
    params.pfnDecodePicture = handleDecodePicture;
    params.pfnDisplayPicture = handleDisplayPicture;
    result = cuvidCreateVideoParser(&_parser, &params);
    CK(result);

    _out_stream = new FFmpegStreamer(AV_CODEC_ID_RAWVIDEO, V_WIDTH, V_HEIGHT, 25, OUTPUT_FILE);
    _demuxer = new FFmpegDemuxer(INPUT_FILE);
    int nVideoBytes = 0, nFrame = 0;
    uint8_t *pVideo = NULL;
    do {
        bool ret = _demuxer->Demux(&pVideo, &nVideoBytes);
        if (nVideoBytes && ret) {
            CUVIDSOURCEDATAPACKET packet = {0};
            packet.payload = pVideo;
            packet.payload_size = nVideoBytes;
            result = cuvidParseVideoData(_parser, &packet);
            CK(result);
        }
    } while (nVideoBytes);

    return 0;
}

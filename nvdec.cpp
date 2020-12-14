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
    printf("\n");
}

// parser callbacks, sps, pps 有变更，需要重新创建 decoder
int handleSequence(void *ud, CUVIDEOFORMAT *format) {
    print_video_format(format);

    // decoder
    CUvideodecoder phDecoder;
    CUVIDDECODECREATEINFO pdci = {0};
    pdci.CodecType = format->codec;
    pdci.ulWidth = format->coded_width;
    pdci.ulHeight = format->coded_height;
    pdci.ulNumDecodeSurfaces = format->min_num_decode_surfaces;
    // pdci.ulTargetWidth = 1920;
    // pdci.ulTargetHeight = 1080;
    pdci.ChromaFormat = format->chroma_format;
    pdci.OutputFormat = cudaVideoSurfaceFormat_NV12;

    // if (format->progressive_sequence)
    //     pdci.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    // else
    //     pdci.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;

    CUresult result = cuvidCreateDecoder(&phDecoder, &pdci);
    CK(result);

    // 要正确的返回，否则或导致在 cuvidParseVideoData 奇怪的错误
    return format->min_num_decode_surfaces;
}

// 解码
int handleDecodePicture(void *ud, CUVIDPICPARAMS * param) {
    CUresult result = cuvidDecodePicture(_decoder, param);
    CK(result);
    return 1;
}

// 获取解码数据
int handleDisplayPicture(void *ud, CUVIDPARSERDISPINFO *pDispInfo) {
    return 1;
    static int nFrame = 0;

    CUVIDPROCPARAMS videoProcessingParameters = {};
    videoProcessingParameters.progressive_frame = pDispInfo->progressive_frame;
    videoProcessingParameters.second_field = pDispInfo->repeat_first_field + 1;
    videoProcessingParameters.top_field_first = pDispInfo->top_field_first;
    videoProcessingParameters.unpaired_field = pDispInfo->repeat_first_field < 0;
    videoProcessingParameters.output_stream = 0;

    CUdeviceptr dpSrcFrame = 0;
    unsigned int nSrcPitch = 0;
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
    char *hostptr = NULL;
    result = cuMemAllocHost((void**)&hostptr, framesize);
    CK(result);

    if (hostptr) {
        result = cuMemcpyDtoH(hostptr, dpSrcFrame, framesize);
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

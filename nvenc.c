/* tools 使用
 *  ffmpeg 截取一帧数据
 *  ffmpeg -i ba.mp4 -frames 1 -c:v rawvideo -pix_fmt nv12 out.yuv
 * 
 *  查看 raw
 *  ffplay -f rawvideo -pix_fmt nv12 -s 1920x1080 -i a.yuv
 * 
 *  播放 h264
 *  ffplay -f h264 -i test.264
 */

#include "nvEncodeAPI.h"
#include <stdio.h>
#include <dlfcn.h>
#include <cuda.h>
#include <string.h>

#define CK(call) do {\
    NVENCSTATUS ret = call;\
    if (NV_ENC_SUCCESS != (ret)) {\
        printf( "[" #call "] call failed, error code: %d, line: %d\n", ret, __LINE__);\
        exit(-1);\
    }\
} while(0);

#define cuCK(call) do {\
    CUresult ret = call;\
    if (CUDA_SUCCESS != (ret)) {\
        printf( "[" #call "] cuda function call failed, error code: %d, line: %d\n", ret, __LINE__);\
        exit(-2);\
    }\
} while(0);

#define INPUT_FILE_NAME "nv12.yuv"
#define IN_FILE_PATH "/root/albert/" INPUT_FILE_NAME
#define OUT_FILE_PATH "/root/albert/out_" INPUT_FILE_NAME ".h264"
#define V_WIDTH 1920
#define V_HEIGHT 1080

static NV_ENCODE_API_FUNCTION_LIST _nvenc = { NV_ENCODE_API_FUNCTION_LIST_VER }; // 一定要初始化一个 ver，要不然会有问题，惨痛的教训
static void *_nvencoder = NULL;
static CUcontext _cuctx = NULL;

void load_nvenc() {
    CK(NvEncodeAPICreateInstance(&_nvenc));
    printf("create interface success!\n");

    cuCK(cuInit(0));

    CUdevice cu_device;
    cuCK(cuDeviceGet(&cu_device, 0));
    printf("get device %d\n", cu_device);

    char szDeviceName[80];
    cuCK(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cu_device));
    printf("gpu name: %s\n", szDeviceName);

    cuCK(cuCtxCreate(&_cuctx, 0, cu_device));

    uint32_t version = 0;
    uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
    CK(NvEncodeAPIGetMaxSupportedVersion(&version));
    printf("%d - %d (%d - %d)\n", version, currentVersion, NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION);

    if (currentVersion > version) {
        printf("nvenc api version mismatch!\n");
        exit(-3);
    }

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS encodeSessionExParams = {0};
    encodeSessionExParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    encodeSessionExParams.device = _cuctx;
    encodeSessionExParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
    encodeSessionExParams.apiVersion = NVENCAPI_VERSION;
    CK(_nvenc.nvEncOpenEncodeSessionEx(&encodeSessionExParams, &_nvencoder));

    printf("open encode session success!\n");
}

int print_info() {
    uint32_t first_count, second_count;
    GUID* guids = NULL;

    // Encode GUIDs
    printf("\n");
    CK(_nvenc.nvEncGetEncodeGUIDCount(_nvencoder, &first_count));

    guids = (GUID*)malloc(first_count * sizeof(GUID));
    uint32_t GUIDCount = 0;
    CK(_nvenc.nvEncGetEncodeGUIDs(_nvencoder, guids, first_count, &second_count));
 
    printf("Encode GUIDs: %d\n", second_count);
    for (int i = 0; i < second_count; i++) {
        printf("\t0x%x\n", guids[i].Data1);
    }
    free(guids);

    // inputformat 
    printf("\n");
    CK(_nvenc.nvEncGetInputFormatCount(_nvencoder, NV_ENC_CODEC_H264_GUID, &first_count));
    NV_ENC_BUFFER_FORMAT* inputFmts = malloc(first_count * sizeof(NV_ENC_BUFFER_FORMAT));
    CK(_nvenc.nvEncGetInputFormats(_nvencoder, NV_ENC_CODEC_H264_GUID, inputFmts, first_count, &second_count));

    printf("Get Input Formats: %d\n", second_count);
    for (int i = 0; i < second_count; i++) {
        printf("\t%p\n", (void*)inputFmts[i]);
    }
    free(inputFmts);

    // preset GUIDs
    printf("\n");
    CK(_nvenc.nvEncGetEncodePresetCount(_nvencoder, NV_ENC_CODEC_H264_GUID, &first_count));
    guids = malloc(sizeof(GUID) * first_count);
    CK(_nvenc.nvEncGetEncodePresetGUIDs(_nvencoder, NV_ENC_CODEC_H264_GUID, guids, first_count, &second_count));
    printf("Preset GUIDs: %d\n", second_count);
    for (int i = 0; i < second_count; i++) {
        printf("\t0x%x\n", guids[i].Data1);
    }
    free(guids);

    // Profile GUIDs
    printf("\n");
    CK(_nvenc.nvEncGetEncodeProfileGUIDCount(_nvencoder, NV_ENC_CODEC_H264_GUID, &first_count));
    guids = malloc(sizeof(GUID) * first_count);
    CK(_nvenc.nvEncGetEncodeProfileGUIDs(_nvencoder, NV_ENC_CODEC_H264_GUID, guids, first_count, &second_count));
    printf("profile GUIDs: %d\n", second_count);
    for (int i = 0; i < second_count; i++) {
        printf("\t0x%x\n", guids[i].Data1);
    }
    free(guids);
    
    // query caps 
    printf("\n");
    printf("Query Caps:\n");
    NV_ENC_CAPS_PARAM capsParam = { NV_ENC_CAPS_PARAM_VER };
    int capsVal;

#define QUERY_CAP(cap) \
    capsParam.capsToQuery = cap; \
    CK(_nvenc.nvEncGetEncodeCaps(_nvencoder, NV_ENC_CODEC_H264_GUID, &capsParam, &capsVal)); \
    printf("\t" #cap ": %d\n", capsVal);

    QUERY_CAP(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_10BIT_ENCODE);
    QUERY_CAP(NV_ENC_CAPS_NUM_MAX_BFRAMES);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_MONOCHROME);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_FMO);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_BDIRECT_MODE);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_STEREO_MVC);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES);
    QUERY_CAP(NV_ENC_CAPS_LEVEL_MAX);
    QUERY_CAP(NV_ENC_CAPS_LEVEL_MIN);
    QUERY_CAP(NV_ENC_CAPS_WIDTH_MAX);
    QUERY_CAP(NV_ENC_CAPS_HEIGHT_MAX);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE);
    QUERY_CAP(NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_LOOKAHEAD);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_MEONLY_MODE);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE);
    QUERY_CAP(NV_ENC_CAPS_SUPPORT_FIELD_ENCODING);
    QUERY_CAP(NV_ENC_CAPS_NUM_ENCODER_ENGINES);
}

void print_enc_config(NV_ENC_CONFIG config) {
    printf("\nNV_ENC_CONFIG:\n");
    printf("profileGUID: 0x%x\n", config.profileGUID.Data1);
    printf("gopLength: %d\n", config.gopLength);
    printf("frameIntervalP: %d\n", config.frameIntervalP);
    printf("monoChromeEncoding: %d\n", config.monoChromeEncoding);
    printf("mvPrecision: 0x%x\n", config.mvPrecision);
    printf("rcParams.xxx\n");
    printf("\n");
}

void init_encoder() {
    NV_ENC_INITIALIZE_PARAMS params;
    params.version = NV_ENC_INITIALIZE_PARAMS_VER;
    params.encodeGUID = NV_ENC_CODEC_H264_GUID;
    params.encodeWidth = V_WIDTH;
    params.encodeHeight = V_HEIGHT;
    params.darWidth = V_WIDTH;
    params.darHeight = V_HEIGHT;
    params.presetGUID = NV_ENC_PRESET_P4_GUID;
    params.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
    params.enableEncodeAsync = 0;
    params.enablePTD = 1;
    params.frameRateNum = 1;
    params.frameRateDen = 15;
    
    // 一定要设置 encodeConfig
    NV_ENC_PRESET_CONFIG presetConfig = { NV_ENC_PRESET_CONFIG_VER, { NV_ENC_CONFIG_VER } };
    CK(_nvenc.nvEncGetEncodePresetConfigEx(_nvencoder, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P4_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY, &presetConfig));
    printf("\nget preset config success， %p\n", params.encodeConfig);
    print_enc_config(presetConfig.presetCfg);

    params.encodeConfig = &presetConfig.presetCfg;
    // params.encodeConfig->profileGUID = NV_ENC_H264_PROFILE_HIGH_444_GUID;
    // params.encodeConfig->gopLength = 1;
    // params.encodeConfig->frameIntervalP = 0;
    // params.encodeConfig->frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;

    CK(_nvenc.nvEncInitializeEncoder(_nvencoder, &params));

    printf("init encoder success!\n");
}

void end_encode() {
    // 通知 encoder 输入结束
    NV_ENC_PIC_PARAMS params = { 0 };
    params.version = NV_ENC_PIC_PARAMS_VER;
    params.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

    CK(_nvenc.nvEncEncodePicture(_nvencoder, &params));
    printf("notify input stream end success!\n");

    // close encoder
    CK(_nvenc.nvEncDestroyEncoder(_nvencoder));
    printf("destroy encoder success!\n");

    // destroy cuda context
    cuCtxDestroy(_cuctx);
}

void encode() {
    // create input buffer
    printf("\n");
    NV_ENC_CREATE_INPUT_BUFFER inputbuf = { NV_ENC_CREATE_INPUT_BUFFER_VER };
    inputbuf.width = V_WIDTH;
    inputbuf.height = V_HEIGHT;
    inputbuf.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
    inputbuf.inputBuffer = NULL;

    CK(_nvenc.nvEncCreateInputBuffer(_nvencoder, &inputbuf));
    printf("create input buffer success, %p\n", inputbuf.inputBuffer);

    // create output stream
    NV_ENC_CREATE_BITSTREAM_BUFFER outbuf = { NV_ENC_CREATE_BITSTREAM_BUFFER_VER };
    CK(_nvenc.nvEncCreateBitstreamBuffer(_nvencoder, &outbuf));
    printf("create output stream success, %p\n", outbuf.bitstreamBuffer);

    // read yuv data
    FILE *fp = fopen(IN_FILE_PATH, "r");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    printf("file size： %lu\n", size);
    fseek(fp, 0, SEEK_SET);

    // lock input 
    NV_ENC_LOCK_INPUT_BUFFER lockBufferParams = { NV_ENC_LOCK_INPUT_BUFFER_VER };
    lockBufferParams.inputBuffer = inputbuf.inputBuffer;
    lockBufferParams.doNotWait = 0;
    lockBufferParams.reservedBitFields = 0;
    CK(_nvenc.nvEncLockInputBuffer(_nvencoder, &lockBufferParams));
    printf("lock success: %p, %d\n", lockBufferParams.bufferDataPtr, lockBufferParams.pitch);

    // copy yuv data to GPU
    fread(lockBufferParams.bufferDataPtr, 1, size, fp);
    printf("writer buffer size: %lu\n", size);

    CK(_nvenc.nvEncUnlockInputBuffer(_nvencoder, inputbuf.inputBuffer));
    printf("unlock success!\n");

    // construct pic params
    NV_ENC_PIC_PARAMS picParams = { 0 };
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.inputWidth = V_WIDTH;
    picParams.inputHeight = V_HEIGHT;

    // 这里如果选择 yuv444 会导致失败 nvEncEncodePicture 返回 12，pix fmt 和 preset guid 要互相配合，这里的关系暂时没搞清楚
    picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
    picParams.inputPitch = lockBufferParams.pitch;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.inputBuffer = inputbuf.inputBuffer;
    picParams.outputBitstream = outbuf.bitstreamBuffer;
    picParams.pictureType = NV_ENC_PIC_TYPE_I;
    picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;

    // submit frame to encode using GPU
    CK(_nvenc.nvEncEncodePicture(_nvencoder, &picParams));
    printf("GPU encode success\n");

    // process output stream
    NV_ENC_LOCK_BITSTREAM bitstream = { 0 };
    bitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
    bitstream.outputBitstream = outbuf.bitstreamBuffer;

    CK(_nvenc.nvEncLockBitstream(_nvencoder, &bitstream));
    printf("Total out bytes: %d\n", bitstream.bitstreamSizeInBytes);
    
    uint8_t *outdata = malloc(bitstream.bitstreamSizeInBytes);
    memcpy(outdata, bitstream.bitstreamBufferPtr, bitstream.bitstreamSizeInBytes);

    CK(_nvenc.nvEncUnlockBitstream(_nvencoder, outbuf.bitstreamBuffer));

    // out ot file
    FILE *out_fp = fopen(OUT_FILE_PATH, "w");
    fwrite(outdata, 1, bitstream.bitstreamSizeInBytes, out_fp);

    // destroy input & output buffer
    printf("\ndestroying\n");
    CK(_nvenc.nvEncDestroyInputBuffer(_nvencoder,  picParams.inputBuffer));
    printf("destroy input buffer success!\n");

    CK(_nvenc.nvEncDestroyBitstreamBuffer(_nvencoder, picParams.outputBitstream));
    printf("destroy output stream success!\n");

    free(outdata);
    fclose(fp);
}

int main() {
    load_nvenc();
    print_info();
    init_encoder();
    encode();
    end_encode();
    return 0;
}

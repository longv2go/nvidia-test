#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>

#define CK(call) do {\
    nvmlReturn_t ret = call;\
    if (NVML_SUCCESS != (ret)) {\
        printf( "[" #call "] call failed, reson: %s, line: %d\n", nvmlErrorString(ret), __LINE__);\
        exit(-1);\
    }\
} while(0);

#define NAME_BUFFER_SIZE 64

void show_info(int gpu_idx) {
    nvmlDevice_t device;
    CK(nvmlDeviceGetHandleByIndex(gpu_idx, &device));

    char name[NAME_BUFFER_SIZE] = {0};
    CK(nvmlDeviceGetName(device, name, NAME_BUFFER_SIZE));
    printf("GPU[%d] name: %s\n", gpu_idx, name);
    
    int major, minor;
    CK(nvmlDeviceGetCudaComputeCapability(device, &major, &minor));
    printf("\tCUDA Capability, major: %d, minor: %d\n", major, minor);

    char serial[NAME_BUFFER_SIZE] = {0};
    CK(nvmlDeviceGetSerial(device, serial, NAME_BUFFER_SIZE));
    printf("\tSerial: %s\n", serial);

    char uuid[NAME_BUFFER_SIZE] = {0};
    CK(nvmlDeviceGetUUID(device, uuid, NAME_BUFFER_SIZE));
    printf("\tUUID: %s\n", uuid);

    char boardnum[NAME_BUFFER_SIZE] = {0};
    CK(nvmlDeviceGetBoardPartNumber(device, boardnum, NAME_BUFFER_SIZE));
    printf("\tBoard Part Num: %s\n", boardnum);

    nvmlEnableState_t bret;
    CK(nvmlDeviceGetDisplayMode(device, &bret));
    printf("\tDisplay Mode: %d\n", bret); // if display monitor connected

    CK(nvmlDeviceGetDisplayActive(device, &bret));
    printf("\tDisplay Active: %d\n", bret);

    CK(nvmlDeviceGetPersistenceMode(device, &bret));
    printf("\tPersistence Mode: %d\n", bret);

    unsigned int tx_value;
    CK(nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_TX_BYTES, &tx_value));
    unsigned int rx_value;
    CK(nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_RX_BYTES, &rx_value));
    printf("\tPCIe Throughput: TX: %d KB/s, RX: %d KB/s\n", tx_value, rx_value);

    // encoder & decoder info
    unsigned int value;
    CK(nvmlDeviceGetEncoderCapacity(device, NVML_ENCODER_QUERY_H264, &value));
    printf("\tEncoder(H264) Cap: %d%\n", value);

    unsigned int sessionCount, averageFps, averageLatency;
    CK(nvmlDeviceGetEncoderStats(device, &sessionCount, &averageFps, &averageLatency));
    printf("\tEncoder Stats, session count: %d, averageFps: %d, averageLatency: %dms\n", sessionCount, averageFps, averageLatency);

    if (sessionCount) {
        nvmlEncoderSessionInfo_t *sessinfos = (nvmlEncoderSessionInfo_t *)malloc(sessionCount * sizeof(nvmlEncoderSessionInfo_t));
        CK(nvmlDeviceGetEncoderSessions(device, &sessionCount, sessinfos));
        printf("\tEncoder Sessions:\n");
        for (int i = 0; i < sessionCount; i++) {
            printf("\t\tSessionId: %d\n", sessinfos[i].sessionId);
            printf("\t\tWidth: %d, Height: %d\n", sessinfos[i].hResolution, sessinfos[i].vResolution);
            printf("\t\taverageFps: %d, averageLatency: %d\n", sessinfos[i].averageFps, sessinfos[i].averageLatency);
        }
        free(sessinfos);
    }
    

    // gpu & mem utilization
    nvmlMemory_t mem_info;
    CK(nvmlDeviceGetMemoryInfo(device, &mem_info));
    printf("\tMemory usage: %lldMiB / %lldMiB\n", mem_info.used / 1024 / 1024, mem_info.total / 1024 / 1024);

    nvmlUtilization_t util;
    CK(nvmlDeviceGetUtilizationRates(device, &util));
    printf("\tMemory util: %d%\n", util.memory);
    printf("\tGPU util: %d%\n", util.gpu);
    
    printf("\n");
}

int main() {
    CK(nvmlInit_v2());

    char version[NAME_BUFFER_SIZE] = {0};
    CK(nvmlSystemGetDriverVersion(version, NAME_BUFFER_SIZE));
    printf("Driver Ver: %s\n", version);

    char nvml_version[NAME_BUFFER_SIZE] = {0};
    CK(nvmlSystemGetNVMLVersion(nvml_version, NAME_BUFFER_SIZE));
    printf("NVML Ver: %s\n", nvml_version);

    int cuda_ver;
    CK(nvmlSystemGetCudaDriverVersion(&cuda_ver));
    printf("CUDA Ver: %d.%d\n", NVML_CUDA_DRIVER_VERSION_MAJOR(cuda_ver), NVML_CUDA_DRIVER_VERSION_MINOR(cuda_ver));

    printf("\n");
    unsigned int device_count;
    CK(nvmlDeviceGetCount(&device_count));

    for (int i = 0; i < device_count; i++) {
        show_info(i);
    }

    return 0;
}

#include <stdio.h>
#include <CL/cl.h>

typedef struct _aaa *aaa_ptr;

#define CL_PATH "cl_test.cl"
#define CL_SOURCE "__kernel void helloworld(__global double* in, __global double* out) { \
    int num = get_global_id(0); \
    out[num] = in[num] / 2.4 *(in[num]/6) ; \
}"

#define CK(ret) if (ret != CL_SUCCESS) printf("%d -- failed, %d\n", __LINE__, ret);

#define GET_PLAT_INFO(key)  ret = clGetPlatformInfo(platforms[0], key, 1024, value, &ret_size);\
    CK(ret);\
    printf(#key ": %s\n", value);

#define GET_DEVICE_INFO_INT(key) ret = clGetDeviceInfo(devices[0], key, sizeof(cl_uint),  &d_value_int, &ret_size);\
        CK(ret);\
        printf(#key ": %d\n", d_value_int);

#define GET_DEVICE_INFO_STR(key) ret = clGetDeviceInfo(devices[0], key, 1024, d_value_str , &ret_size);\
        CK(ret);\
        printf(#key ": %s\n", d_value_str);

#define GET_DEVICE_INFO_SIZE(key) ret = clGetDeviceInfo(devices[0], key, sizeof(size_t),  &d_value_size, &ret_size);\
        CK(ret);\
        printf(#key ": %lu\n", d_value_size);

void cl_ctx_callback(const char *errinfo, const void *private_info, size_t cb, void *user_data) {
    printf("cl context error: %s\n", errinfo);
}


int main() {
    cl_int num_platforms;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    CK(ret);
    printf(" has %d cl platforms\n", num_platforms);

    cl_platform_id *platforms = malloc(num_platforms * sizeof(cl_platform_id));
    ret = clGetPlatformIDs(num_platforms, platforms, &num_platforms);
    CK(ret);

    char value[1024] = {0};
    size_t ret_size = 0;
    ret = clGetPlatformInfo(platforms[0], CL_PLATFORM_NAME, 1024, value, &ret_size);
    CK(ret);
    printf("platform name: %s\n", value);

    ret = clGetPlatformInfo(platforms[0], CL_PLATFORM_VENDOR, 1024, value, &ret_size);
    CK(ret);
    printf("platform vendor: %s\n", value);

    GET_PLAT_INFO(CL_PLATFORM_VERSION);

    // devices

    cl_device_id devices[10] = {0};
    cl_uint num_devices = 0;
    ret = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, 10, devices, &num_devices);
    CK(ret);
    printf("get num_devies: %d\n", num_devices);

    char d_value_str[1024] = {0};
    cl_uint d_value_int;
    size_t d_value_size;

    GET_DEVICE_INFO_INT(CL_DEVICE_VENDOR_ID);
    GET_DEVICE_INFO_INT(CL_DEVICE_MAX_COMPUTE_UNITS);
    GET_DEVICE_INFO_INT(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS);
    GET_DEVICE_INFO_INT(CL_DEVICE_ADDRESS_BITS);

    GET_DEVICE_INFO_STR(CL_DEVICE_NAME);
    GET_DEVICE_INFO_STR(CL_DRIVER_VERSION);
    GET_DEVICE_INFO_STR(CL_DEVICE_PROFILE);
    GET_DEVICE_INFO_STR(CL_DEVICE_VERSION);
    GET_DEVICE_INFO_STR(CL_DEVICE_OPENCL_C_VERSION);

    GET_DEVICE_INFO_SIZE(CL_DEVICE_MAX_WORK_GROUP_SIZE);

    // context

    cl_int error;
    cl_context context = clCreateContext(NULL, 4, devices, cl_ctx_callback, (void*)0x123, &error);
    CK(error);

    cl_uint ref_cnt;
    ret = clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT, sizeof(cl_uint), &ref_cnt, NULL);
    CK(ret);
    printf("context reference count: %d\n", ref_cnt);

    // command queues

    cl_command_queue queue = clCreateCommandQueue(context, devices[0], CL_QUEUE_PROFILING_ENABLE, &error);
    CK(error);

    cl_device_id q_device;
    ret = clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &q_device, NULL);
    CK(ret);
    printf("device - queued device, %p - %p\n", devices[0], q_device);

    // buffer

    size_t buffer_size = 1  * 1000 * 1000 ;
    printf("buffer size: %lu\n", buffer_size);

    void *host_buffer = malloc(buffer_size);
    cl_mem mem = clCreateBuffer(context, CL_MEM_READ_WRITE, buffer_size, host_buffer, &error);
    CK(error);

    void *ptr;
    ret = clGetMemObjectInfo(mem, CL_MEM_HOST_PTR, sizeof(void*), &ptr, NULL);
    CK(ret);

    printf("get host ptr: %p - %p\n", ptr, host_buffer);


    cl_event event;
    ret = clEnqueueWriteBuffer(queue, mem, CL_TRUE, 0, buffer_size, host_buffer, 0, NULL,  &event);
    CK(ret);

    clFinish(queue);


    // program
    const char *source_ptr = CL_SOURCE;
    size_t source_sizes[] = {sizeof(CL_SOURCE)};
    cl_program prog = clCreateProgramWithSource(context, 1, &source_ptr, source_sizes, &error);
    CK(error);

    ret = clBuildProgram(prog, 1, devices, NULL, NULL, NULL);
    CK(ret);

    cl_kernel kernel = clCreateKernel(prog, "helloworld", &error);
    CK(error);

    size_t work_size[3];
    //ret = clGetKernelWorkGroupInfo(kernel, devices[0], CL_KERNEL_GLOBAL_WORK_SIZE, sizeof(work_size), work_size, &ret_size);
    CK(ret);

    // parameters
    const int NUM=512000;
    double *input = malloc(sizeof(double) * NUM);
    for (int i = 0; i < NUM; i++) {
        input[i] = i;
    }
    double *output = malloc(sizeof(double) * NUM);

    cl_mem inputBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(double) * NUM, input, &error);
    CK(error);
    cl_mem outputBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(double) * NUM, NULL, NULL);

    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), &inputBuffer);
    CK(ret);
    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), &outputBuffer);
    CK(ret);

    size_t global_work_size[1] = {NUM};
    cl_event queue_event;

    ret = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, NULL, 0, NULL, &queue_event);
    CK(ret);
    clWaitForEvents(1, &queue_event);
    clReleaseEvent(event);

    ret = clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0, NUM, output, 0, NULL, NULL);
    CK(ret);

    int idx = 400;
    printf("output[%d] = %lf\n", idx, output[idx]);

    // release
    clReleaseKernel(kernel);
    clReleaseProgram(prog);
    clReleaseMemObject(inputBuffer);
    clReleaseMemObject(outputBuffer);
    clReleaseMemObject(mem);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    free(output);
    free(input);

    return 0;
}

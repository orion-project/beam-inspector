/*

Base OpenCL example
https://www.nersc.gov/assets/pubs_presos/MattsonTutorialSC14.pdf
https://ulhpc-tutorials.readthedocs.io/en/latest/gpu/opencl
https://programmerclick.com/article/47811146604

*/
#include <iostream>

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/cl.hpp>

using namespace cl;
using namespace std;

int main()
{
    std::vector<Platform> platforms;
    Platform::get(&platforms);
    if (platforms.empty()) {
        cerr << "ERR: no platforms found" << endl;
        return 1;
    }
    cout << "platforms: " << platforms.size() << endl;
    for (auto i = 0; i < platforms.size(); i++) {
        const auto& platform = platforms[i];
        cout << "platform " << i << ":" << endl
            << "    name: " << platform.getInfo<CL_PLATFORM_NAME>() << endl
            << "    vendor: " << platform.getInfo<CL_PLATFORM_VENDOR>() << endl
            << "    version: " << platform.getInfo<CL_PLATFORM_VERSION>() << endl
            << "    profile: " << platform.getInfo<CL_PLATFORM_PROFILE>() << endl
        ;
    }

    const auto platformIdx = 0;
    const auto& platform = platforms[platformIdx];
    cout << "using platform " << platformIdx << endl;

    std::vector<Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
    if (devices.empty()) {
        cerr << "ERR: no devices found" << endl;
        return 1;
    }
    cout << "devices: " << devices.size() << endl;
    for (auto i = 0; i < devices.size(); i++) {
        const auto& device = devices[i];
        cout << "device " << i << ":" << endl
            << "    type: " << device.getInfo<CL_DEVICE_TYPE>() << endl
            << "    name: " << device.getInfo<CL_DEVICE_NAME>() << endl
            << "    vendor: " << device.getInfo<CL_DEVICE_VENDOR>() << endl
            << "    version: " << device.getInfo<CL_DRIVER_VERSION>() << endl
            << "    profile: " << device.getInfo<CL_DEVICE_PROFILE>() << endl
            << "    available: " << device.getInfo<CL_DEVICE_AVAILABLE>() << endl
            << "    GLOBAL_MEM_SIZE: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() << endl
            << "    LOCAL_MEM_SIZE: " << device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() << endl
            << "    MAX_SAMPLERS: " << device.getInfo<CL_DEVICE_MAX_SAMPLERS>() << endl
            << "    MAX_COMPUTE_UNITS: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << endl
            << "    MAX_WORK_ITEM_DIMENSIONS: " << device.getInfo<CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS>() << endl
            << "    MAX_WORK_GROUP_SIZE: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl
        ;
        auto workItemSizes = device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
        cout << "    MAX_WORK_ITEM_SIZES:";
        for (const auto& sz : workItemSizes)
            cout << " " << sz;
        cout << endl;
        cout
            << "    PREFERRED_VECTOR_WIDTH_DOUBLE: " << device.getInfo<CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE>() << endl
            << "    PREFERRED_VECTOR_WIDTH_INT: " << device.getInfo<CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT>() << endl
        ;
    }

    const auto deviceIdx = 0;
    const auto& device = devices[deviceIdx];
    cout << "using device " << deviceIdx << endl;

    string code = R"(
        void kernel simple_add(global const int* A, global const int* B, global int* C) {
            int i = get_global_id(0);
            C[i] = A[i] + B[i];
        }
    )";
    Program::Sources sources;
    sources.push_back(code);

    Context context({device});
    Program program(context, sources);
    if (program.build() != CL_SUCCESS) {
        cerr << "ERR: kernel build failed: " << endl
            << "    status: " << program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device) << endl
            << "    log: " <<  program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << endl
        ; 
        return 1;
    }
    cout << "kernel built" << endl;

    const auto arraySize = 10;
    int A_h[arraySize] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int B_h[arraySize] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
    int C_h[arraySize];

    Buffer A_d(context, CL_MEM_READ_ONLY, sizeof(int)*arraySize);
    Buffer B_d(context, CL_MEM_READ_ONLY, sizeof(int)*arraySize);
    Buffer C_d(context, CL_MEM_WRITE_ONLY, sizeof(int)*arraySize);

    CommandQueue queue(context, device);
    queue.enqueueWriteBuffer(A_d, CL_TRUE, 0, sizeof(int)*arraySize, A_h);
    queue.enqueueWriteBuffer(B_d, CL_TRUE, 0, sizeof(int)*arraySize, B_h);

    Kernel kernel(program, "simple_add");
    cout << "kernel:" << endl
        << "    WORK_GROUP_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device) << endl
        << "    LOCAL_MEM_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(device) << endl
        << "    PRIVATE_MEM_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_PRIVATE_MEM_SIZE>(device) << endl
    ;

    KernelFunctor<Buffer, Buffer, Buffer> simpleAdd(kernel);
    simpleAdd(EnqueueArgs(queue, NDRange(arraySize)), A_d, B_d, C_d).wait();

    queue.enqueueReadBuffer(C_d, CL_TRUE, 0, sizeof(int)*arraySize, C_h);

    cout << "Results:";
    for (int i : C_h)
        cout << " " << i;
    cout << endl;

    return 0;
}

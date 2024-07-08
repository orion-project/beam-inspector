/*

Performance comparison between CPU and GPU for image averaging.

GCC 11.2x64 -O3
Intel i7-2600 CPU 3.40GHz
NVIDIA GeForce RTX 3060
Frames: 100
Image: 2592x2048
Elapsed CPU: 0.521s, FPS: 191.8
Elapsed GPU: 1.694s, FPS: 59.0
    no copy: 0.137s, FPS: 731.2

*/
#include <iostream>
#include <chrono>

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/cl.hpp>

#define FRAMES 100
#define W 2592
#define H 2048

using namespace cl;
using namespace std;
using duration = std::chrono::duration<double>;
using timer = std::chrono::high_resolution_clock;

int main()
{
    Context context(CL_DEVICE_TYPE_GPU);
    auto device = context.getInfo<CL_CONTEXT_DEVICES>()[0];
    cout << device.getInfo<CL_DEVICE_NAME>() << endl;

    CommandQueue queue(context);

    Program program(context, {R"(
        kernel void moving_avg(global int *a, global const int *b) {
            int i = get_global_id(0);
            a[i] = convert_int_rtz( convert_float(a[i])*0.9 + convert_float(b[i])*0.1 );
        }
    )"}, false);
    if (program.build() != CL_SUCCESS) {
        cerr << "ERR: kernel build failed: " << endl
            << "    status: " << program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device) << endl
            << "    log: " <<  program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << endl
        ; 
        return 1;
    }

    KernelFunctor<Buffer, Buffer> moving_avg(program, "moving_avg");
    Buffer A_d(context, CL_MEM_READ_WRITE, sizeof(int)*W*H);
    Buffer B_d(context, CL_MEM_READ_ONLY, sizeof(int)*W*H);

    std::vector<int> A_h_cpu(W*H);
    std::vector<int> A_h_gpu(W*H);
    std::vector<int> B_h(W*H);

    for (int i = 0; i < W*H; i++)
        A_h_gpu[i] = A_h_cpu[i] = rand() % 255;
    cl::copy(queue, A_h_gpu.begin(), A_h_gpu.end(), A_d);

    printf("Frames: %d\n", FRAMES);
    printf("Image: %dx%d\n", W, H);
    duration elapsed_cpu;
    duration elapsed_gpu, elapsed_gpu0;
    for (int frame = 0; frame < FRAMES; frame++) {
        for (int i = 0; i < W*H; i++) {
            B_h[i] = rand() % 255;
        }
        {
            auto tm = timer::now();
            for (int i = 0; i < W*H; i++)
                A_h_cpu[i] = float(A_h_cpu[i])*0.9 + float(B_h[i])*0.1;
            elapsed_cpu += timer::now() - tm;
        }
        {
            auto tm = timer::now();
            cl::copy(queue, B_h.begin(), B_h.end(), B_d);
            auto tm0 = timer::now();
            moving_avg(EnqueueArgs(queue, NDRange(W*H)), A_d, B_d).wait();
            elapsed_gpu0 += timer::now() - tm0;
            cl::copy(queue, A_d, A_h_gpu.begin(), A_h_gpu.end());
            elapsed_gpu += timer::now() - tm;
        }
        for (int i = 0; i < W*H; i++)
            if (A_h_cpu[i] != A_h_gpu[i]) {
                printf("Frame %d: CPU and GPU results do not match: %d != %d\n", frame, A_h_cpu[i], A_h_gpu[i]);
                break;
            }
    }
    printf("Elapsed CPU: %.3fs, FPS: %.1f\n", elapsed_cpu.count(), FRAMES/elapsed_cpu.count());
    printf("Elapsed GPU: %.3fs, FPS: %.1f\n", elapsed_gpu.count(), FRAMES/elapsed_gpu.count());
    printf("    no copy: %.3fs, FPS: %.1f\n", elapsed_gpu0.count(), FRAMES/elapsed_gpu0.count());
    return 0;
}

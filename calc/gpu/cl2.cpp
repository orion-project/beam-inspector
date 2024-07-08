/*

Performance comparison of matrix multiplication with different optimizations
https://www.nersc.gov/assets/pubs_presos/MattsonTutorialSC14.pdf

                B= ┌──────────┐
                   │          │
                   │          │
                   │  H x W   │
                   │          │
                   │          │
   A=              └──────────┘
  ┌──────────────┐ ┌──────────┐
  │              │ │          │
  │    W x H     │ │  H x H   │ =C
  │              │ │          │
  └──────────────┘ └──────────┘

GCC 11.2x64 -O3
Intel i7-2600 CPU 3.40GHz
NVIDIA GeForce RTX 3060
Frames: 10
Image: 2592x2048
**** mult_matr_1
Elapsed GPU: 5.119s, FPS: 2.0
    no copy: 4.855s, FPS: 2.1
**** mult_matr_2
Elapsed GPU: 15.835s, FPS: 0.6
    no copy: 15.553s, FPS: 0.6
**** mult_matr_3
Elapsed GPU: 2.776s, FPS: 3.6
    no copy: 2.506s, FPS: 4.0
**** mult_matr_4
Elapsed GPU: 2.621s, FPS: 3.8
    no copy: 2.377s, FPS: 4.2

*/
#include <iostream>
#include <chrono>
#include <optional>

#define CL_HPP_TARGET_OPENCL_VERSION 300
#include <CL/cl.hpp>

#define WORK_ITEMS_PER_GROUP 512
#define LOCAL_DIM NDRange(WORK_ITEMS_PER_GROUP)
//#define MEASURE_CPU
#ifdef MEASURE_CPU
    #define FRAMES 1
    #define W 1024
    #define H 512
    #define MEASURE_GPU_4
#else
    #define FRAMES 10
    #define W 2592
    #define H 2048
    #define MEASURE_GPU_1
    #define MEASURE_GPU_2
    #define MEASURE_GPU_3
    #define MEASURE_GPU_4
#endif

using namespace cl;
using namespace std;
using duration = std::chrono::duration<double>;
using timer = std::chrono::high_resolution_clock;

template <typename... Args>
KernelFunctor<Args...> make_functor(Context &context, string name, string code) {
    auto device = context.getInfo<CL_CONTEXT_DEVICES>()[0];
    Program program(context, {code}, false);
    if (program.build() != CL_SUCCESS) {
        cerr << "ERR: kernel build failed: " << endl
            << "    status: " << program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device) << endl
            << "    log: " <<  program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << endl
        ; 
        exit(1);
    }
    KernelFunctor<Args...> functor(program, name);
    return functor;
}

string str_replace(string str, const string& from, const string& to) {
    size_t start_pos = str.find(from);
    if (start_pos != string::npos)
        str.replace(start_pos, from.length(), to);
    return str;
}

int main()
{
    Context context(CL_DEVICE_TYPE_GPU);
    auto device = context.getInfo<CL_CONTEXT_DEVICES>()[0];
    cout << device.getInfo<CL_DEVICE_NAME>() << endl;

    CommandQueue queue(context);

#ifdef MEASURE_GPU_1
    // One work item per result pixel
    auto mult_matr_1 = make_functor<int, int, Buffer, Buffer, Buffer>(context, "mult_matr_1", R"(
        kernel void mult_matr_1(const int W, const int H, global const int *A, global const int *B, global int *C) {
            int i = get_global_id(0);
            int j = get_global_id(1);
            int sum = 0;
            for (int k = 0; k < W; k++) {
                sum += A[i*H + k] * B[k*H + j];
            }
            C[i*H + j] = sum;
        }
    )");
#endif
#ifdef MEASURE_GPU_2
    // One work item per result row
    auto mult_matr_2 = make_functor<int, int, Buffer, Buffer, Buffer>(context, "mult_matr_2", R"(
        kernel void mult_matr_2(const int W, const int H, global const int *A, global const int *B, global int *C) {
            int i = get_global_id(0);
            for (int j = 0; j < H; j++) {
                int sum = 0;
                for (int k = 0; k < W; k++) {
                    sum += A[i*H + k] * B[k*H + j];
                }
                C[i*H + j] = sum;
            }
        }
    )");
#endif
#ifdef MEASURE_GPU_3
    // One work item per result row + row of A in private memory
    auto mult_matr_3 = make_functor<int, int, Buffer, Buffer, Buffer>(context, "mult_matr_3", str_replace(R"(
        kernel void mult_matr_3(const int W, const int H, global const int *A, global const int *B, global int *C) {
            int i = get_global_id(0);
            int A_row[$W];
            for (int k = 0; k < W; k++) {
                A_row[k] = A[i*H + k];
            }
            for (int j = 0; j < H; j++) {
                int sum = 0;
                for (int k = 0; k < W; k++) {
                    sum += A_row[k] * B[k*H + j];
                }
                C[i*H + j] = sum;
            }
        }
    )", "$W", to_string(W)));
#endif
#ifdef MEASURE_GPU_4
    // One work item per result row + row of A in private memory + col of B in local memory
    auto mult_matr_4 = make_functor<int, int, Buffer, Buffer, Buffer, LocalSpaceArg>(context, "mult_matr_4", str_replace(R"(
        kernel void mult_matr_4(const int W, const int H, global const int *A, global const int *B, global int *C, local int *B_col) {
            int i = get_global_id(0);
            int A_row[$W];
            for (int k = 0; k < W; k++) {
                A_row[k] = A[i*H + k];
            }
            int item_id = get_local_id(0);
            int group_size = get_local_size(0);
            for (int j = 0; j < H; j++) {
                // Every work item assigns a part of shared colum
                for (int k = item_id; k < W; k += group_size) {
                    B_col[k] = B[k*H + j];
                }
                // All work items run in parallel and C-item calc starts after all of them finished
                barrier(CLK_LOCAL_MEM_FENCE);

                int sum = 0;
                for (int k = 0; k < W; k++) {
                    sum += A_row[k] * B_col[k];
                }
                C[i*H + j] = sum;

                // Each B-colum processed in sequence for not to spoil shared copy
                barrier(CLK_LOCAL_MEM_FENCE);
            }
        }
    )", "$W", to_string(W)));
#endif

    std::vector<int> A_h(W*H);
    std::vector<int> B_h(H*W);
    std::vector<int> C_h_cpu(H*H);
    std::vector<int> C_h_gpu(H*H);
    Buffer A_d(context, CL_MEM_READ_ONLY, sizeof(int)*W*H);
    Buffer B_d(context, CL_MEM_READ_ONLY, sizeof(int)*H*W);
    Buffer C_d(context, CL_MEM_WRITE_ONLY, sizeof(int)*H*H);
#ifdef MEASURE_GPU_4
    LocalSpaceArg B_col = Local(sizeof(int)*W);
#endif

    printf("Frames: %d\n", FRAMES);
    printf("Image: %dx%d\n", W, H);
    duration elapsed_cpu;
    duration elapsed_gpu1, elapsed_gpu1_0;
    duration elapsed_gpu2, elapsed_gpu2_0;
    duration elapsed_gpu3, elapsed_gpu3_0;
    duration elapsed_gpu4, elapsed_gpu4_0;
    for (int frame = 0; frame < FRAMES; frame++) {
        for (int i = 0; i < W*H; i++) {
            A_h[i] = rand() % 255;
            B_h[i] = rand() % 255;
        }
    #ifdef MEASURE_CPU
        auto tm_cpu = timer::now();
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < H; j++) {
                int sum = 0;
                for (int k = 0; k < W; k++) {
                    sum += A_h[i*H + k] * B_h[k*H + j];
                }
                C_h_cpu[i*H + j] = sum;
            }
        }
        elapsed_cpu += timer::now() - tm_cpu;
    #endif
    #define MULT_MATR(n, functor) { \
        static bool reported##n = false; \
        auto tm = timer::now(); \
        cl::copy(queue, A_h.begin(), A_h.end(), A_d); \
        cl::copy(queue, B_h.begin(), B_h.end(), B_d); \
        auto tm0 = timer::now(); \
        functor.wait(); \
        elapsed_gpu##n##_0 += timer::now() - tm0; \
        cl::copy(queue, C_d, C_h_gpu.begin(), C_h_gpu.end()); \
        elapsed_gpu##n += timer::now() - tm; \
        Kernel kernel = mult_matr_##n.getKernel(); \
        if (!reported##n) { \
            reported##n = true; \
            cout << "mult_matr_" #n << ':' << endl \
                << "    LOCAL_MEM_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(device) << endl \
                << "    PRIVATE_MEM_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_PRIVATE_MEM_SIZE>(device) << endl \
                << "    WORK_GROUP_SIZE: " << kernel.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device) << endl \
            ; \
        } \
    }
    #ifdef MEASURE_GPU_1
        MULT_MATR(1, mult_matr_1(EnqueueArgs(queue, NDRange(H, H), LOCAL_DIM), W, H, A_d, B_d, C_d));
    #endif
    #ifdef MEASURE_GPU_2
        MULT_MATR(2, mult_matr_2(EnqueueArgs(queue, NDRange(H), LOCAL_DIM), W, H, A_d, B_d, C_d));
    #endif
    #ifdef MEASURE_GPU_3
        MULT_MATR(3, mult_matr_3(EnqueueArgs(queue, NDRange(H), LOCAL_DIM), W, H, A_d, B_d, C_d));
    #endif
    #ifdef MEASURE_GPU_4
        MULT_MATR(4, mult_matr_4(EnqueueArgs(queue, NDRange(H), LOCAL_DIM), W, H, A_d, B_d, C_d, B_col));
    #endif
    #ifdef MEASURE_CPU
        for (int i = 0; i < H*H; i++)
            if (C_h_cpu[i] != C_h_gpu[i]) {
                printf("Frame %d: CPU and GPU results do not match: %d != %d\n", frame, C_h_cpu[i], C_h_gpu[i]);
                break;
            }
    #endif
    }
#define PRINT_GPU_RES(n) { \
    printf("**** mult_matr_" #n "\n"); \
    printf("Elapsed GPU: %.3fs, FPS: %.1f\n", elapsed_gpu##n.count(), FRAMES/elapsed_gpu##n.count()); \
    printf("    no copy: %.3fs, FPS: %.1f\n", elapsed_gpu##n##_0.count(), FRAMES/elapsed_gpu##n##_0.count()); \
}
#ifdef MEASURE_CPU
    printf("Elapsed CPU: %.3fs, FPS: %.1f\n", elapsed_cpu.count(), FRAMES/elapsed_cpu.count());
#endif
#ifdef MEASURE_GPU_1
    PRINT_GPU_RES(1)
#endif
#ifdef MEASURE_GPU_2
    PRINT_GPU_RES(2)
#endif
#ifdef MEASURE_GPU_3
    PRINT_GPU_RES(3)
#endif
#ifdef MEASURE_GPU_4
    PRINT_GPU_RES(4)
#endif
    return 0;
}

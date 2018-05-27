#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
#include <vector>
#include <cstdlib>

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

namespace {
template<typename T>
class Scope_exit {
    T t;
public:
    Scope_exit(const T &tt): t(tt) {}
    Scope_exit(const Scope_exit &) = delete;
    Scope_exit(Scope_exit &&) = delete;
    Scope_exit &operator = (const Scope_exit &) = delete;
    Scope_exit &operator = (Scope_exit &&) = delete;
    ~Scope_exit() { t(); }
};

struct Work {
    uint32_t init_state[4];
    uint32_t mask[4];
    uint32_t data[16];
    uint32_t mutable_index;
    uint64_t count;
};

std::istream &operator >> (std::istream &in, Work &work) {
    for (uint32_t &u : work.init_state)
        in >> u;
    for (uint32_t &u : work.mask)
        in >> u;
    for (uint32_t &u : work.data)
        in >> u;
    in >> work.mutable_index >> work.count;
    if (work.mutable_index >= std::size(work.data))
        in.setstate(std::ios_base::failbit);
    return in;
}

constexpr const char *md5rush_source = R"(
struct Work {
    uint init_state[4];
    uint mask[4];
    uint data[16];
    uint mutable_index;
    ulong count; // unused
};

__kernel void md5rush(__constant struct Work *work,
        volatile __global uint *found,
        volatile __global uint *index) {
    uint a = work->init_state[0];
    uint b = work->init_state[1];
    uint c = work->init_state[2];
    uint d = work->init_state[3];
#define MD5_ITERATION(F, G, K, S) \
    do { \
        uint f = (F) + a + (K) + work->data[(G)] + \
            ((G) == work->mutable_index ? get_global_id(0) : 0); \
        a = d; \
        d = c; \
        c = b; \
        b += (f << (S)) | (f >> (32 - (S))); \
    } while (0)
    MD5_ITERATION((b & c) | (~b & d),  0, 3614090360,  7);
    MD5_ITERATION((b & c) | (~b & d),  1, 3905402710, 12);
    MD5_ITERATION((b & c) | (~b & d),  2,  606105819, 17);
    MD5_ITERATION((b & c) | (~b & d),  3, 3250441966, 22);
    MD5_ITERATION((b & c) | (~b & d),  4, 4118548399,  7);
    MD5_ITERATION((b & c) | (~b & d),  5, 1200080426, 12);
    MD5_ITERATION((b & c) | (~b & d),  6, 2821735955, 17);
    MD5_ITERATION((b & c) | (~b & d),  7, 4249261313, 22);
    MD5_ITERATION((b & c) | (~b & d),  8, 1770035416,  7);
    MD5_ITERATION((b & c) | (~b & d),  9, 2336552879, 12);
    MD5_ITERATION((b & c) | (~b & d), 10, 4294925233, 17);
    MD5_ITERATION((b & c) | (~b & d), 11, 2304563134, 22);
    MD5_ITERATION((b & c) | (~b & d), 12, 1804603682,  7);
    MD5_ITERATION((b & c) | (~b & d), 13, 4254626195, 12);
    MD5_ITERATION((b & c) | (~b & d), 14, 2792965006, 17);
    MD5_ITERATION((b & c) | (~b & d), 15, 1236535329, 22);
    MD5_ITERATION((d & b) | (~d & c),  1, 4129170786,  5);
    MD5_ITERATION((d & b) | (~d & c),  6, 3225465664,  9);
    MD5_ITERATION((d & b) | (~d & c), 11,  643717713, 14);
    MD5_ITERATION((d & b) | (~d & c),  0, 3921069994, 20);
    MD5_ITERATION((d & b) | (~d & c),  5, 3593408605,  5);
    MD5_ITERATION((d & b) | (~d & c), 10,   38016083,  9);
    MD5_ITERATION((d & b) | (~d & c), 15, 3634488961, 14);
    MD5_ITERATION((d & b) | (~d & c),  4, 3889429448, 20);
    MD5_ITERATION((d & b) | (~d & c),  9,  568446438,  5);
    MD5_ITERATION((d & b) | (~d & c), 14, 3275163606,  9);
    MD5_ITERATION((d & b) | (~d & c),  3, 4107603335, 14);
    MD5_ITERATION((d & b) | (~d & c),  8, 1163531501, 20);
    MD5_ITERATION((d & b) | (~d & c), 13, 2850285829,  5);
    MD5_ITERATION((d & b) | (~d & c),  2, 4243563512,  9);
    MD5_ITERATION((d & b) | (~d & c),  7, 1735328473, 14);
    MD5_ITERATION((d & b) | (~d & c), 12, 2368359562, 20);
    MD5_ITERATION(b ^ c ^ d         ,  5, 4294588738,  4);
    MD5_ITERATION(b ^ c ^ d         ,  8, 2272392833, 11);
    MD5_ITERATION(b ^ c ^ d         , 11, 1839030562, 16);
    MD5_ITERATION(b ^ c ^ d         , 14, 4259657740, 23);
    MD5_ITERATION(b ^ c ^ d         ,  1, 2763975236,  4);
    MD5_ITERATION(b ^ c ^ d         ,  4, 1272893353, 11);
    MD5_ITERATION(b ^ c ^ d         ,  7, 4139469664, 16);
    MD5_ITERATION(b ^ c ^ d         , 10, 3200236656, 23);
    MD5_ITERATION(b ^ c ^ d         , 13,  681279174,  4);
    MD5_ITERATION(b ^ c ^ d         ,  0, 3936430074, 11);
    MD5_ITERATION(b ^ c ^ d         ,  3, 3572445317, 16);
    MD5_ITERATION(b ^ c ^ d         ,  6,   76029189, 23);
    MD5_ITERATION(b ^ c ^ d         ,  9, 3654602809,  4);
    MD5_ITERATION(b ^ c ^ d         , 12, 3873151461, 11);
    MD5_ITERATION(b ^ c ^ d         , 15,  530742520, 16);
    MD5_ITERATION(b ^ c ^ d         ,  2, 3299628645, 23);
    MD5_ITERATION(c ^ (b | ~d)      ,  0, 4096336452,  6);
    MD5_ITERATION(c ^ (b | ~d)      ,  7, 1126891415, 10);
    MD5_ITERATION(c ^ (b | ~d)      , 14, 2878612391, 15);
    MD5_ITERATION(c ^ (b | ~d)      ,  5, 4237533241, 21);
    MD5_ITERATION(c ^ (b | ~d)      , 12, 1700485571,  6);
    MD5_ITERATION(c ^ (b | ~d)      ,  3, 2399980690, 10);
    MD5_ITERATION(c ^ (b | ~d)      , 10, 4293915773, 15);
    MD5_ITERATION(c ^ (b | ~d)      ,  1, 2240044497, 21);
    MD5_ITERATION(c ^ (b | ~d)      ,  8, 1873313359,  6);
    MD5_ITERATION(c ^ (b | ~d)      , 15, 4264355552, 10);
    MD5_ITERATION(c ^ (b | ~d)      ,  6, 2734768916, 15);
    MD5_ITERATION(c ^ (b | ~d)      , 13, 1309151649, 21);
    MD5_ITERATION(c ^ (b | ~d)      ,  4, 4149444226,  6);
    MD5_ITERATION(c ^ (b | ~d)      , 11, 3174756917, 10);
    MD5_ITERATION(c ^ (b | ~d)      ,  2,  718787259, 15);
    MD5_ITERATION(c ^ (b | ~d)      ,  9, 3951481745, 21);
#undef MD5_ITERATION
    a += work->init_state[0];
    b += work->init_state[1];
    c += work->init_state[2];
    d += work->init_state[3];
    a &= work->mask[0];
    b &= work->mask[1];
    c &= work->mask[2];
    d &= work->mask[3];
    if ((a | b | c | d) == 0) {
        atom_inc(found);
        atom_min(index, get_global_id(0));
    }
}
)";
}

int main() {
    cl_int err;

    cl_device_id device;
    cl_uint num_devices;
    err = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_DEFAULT,
            1, &device, &num_devices);
    if (err != CL_SUCCESS) {
        std::cerr << "Error getting default device: " << err << std::endl;
        return 1;
    }
    if (num_devices == 0) {
        std::cerr << "No default device found." << std::endl;
        return 1;
    }

    cl_context context = clCreateContext(nullptr, 1, &device,
            nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating context: " << err << std::endl;
        return 1;
    }
    Scope_exit release_context([context] {
        cl_int err2 = clReleaseContext(context);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing context: " << err2 << std::endl;
    });

    const char *sources[] = { md5rush_source };
    cl_program program = clCreateProgramWithSource(
            context, 1, sources, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating program: " << err << std::endl;
        return 1;
    }
    Scope_exit release_program([program] {
        cl_int err2 = clReleaseProgram(program);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing program: " << err2 << std::endl;
    });

    err = clBuildProgram(program, 0, nullptr, "", nullptr, nullptr);
    if (err != CL_SUCCESS) {
        std::cerr << "Error building program: " << err << std::endl;

        size_t log_size;
        err = clGetProgramBuildInfo(program, device,
                CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        if (err != CL_SUCCESS) {
            std::cerr << "Error getting build log size: " << err << std::endl;
            return 1;
        }

        std::vector<char> log(log_size + 1); 
        err = clGetProgramBuildInfo(program, device,
                CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error getting build log: " << err << std::endl;
            return 1;
        }

        std::cerr << log.data() << std::endl;
        return 1;
    }

    cl_kernel kernel_md5rush = clCreateKernel(program, "md5rush", &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating kernel: " << err << std::endl;
        return 1;
    }
    Scope_exit release_kernel_md5rush([kernel_md5rush] {
        cl_int err2 = clReleaseKernel(kernel_md5rush);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing kernel: " << err2 << std::endl;
    });

    cl_command_queue cmdqueue = clCreateCommandQueue(context, device, 0, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating command queue: " << err << std::endl;
        return 1;
    }
    Scope_exit release_cmdqueue([cmdqueue] {
        cl_int err2 = clReleaseCommandQueue(cmdqueue);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing command queue: " << err2 << std::endl;
    });

    cl_mem mem_work = clCreateBuffer(context,
            CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
            sizeof(Work), nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating buffer 1: " << err << std::endl;
        return 1;
    }
    Scope_exit release_mem_work([mem_work] {
        cl_int err2 = clReleaseMemObject(mem_work);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing buffer 0: " << err2 << std::endl;
    });

    cl_mem mem_found = clCreateBuffer(context, CL_MEM_READ_WRITE,
            sizeof(uint32_t), nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating buffer 2: " << err << std::endl;
        return 1;
    }
    Scope_exit release_mem_found([mem_found] {
        cl_int err2 = clReleaseMemObject(mem_found);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing buffer 1: " << err2 << std::endl;
    });

    cl_mem mem_index = clCreateBuffer(context, CL_MEM_READ_WRITE,
            sizeof(uint32_t), nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Error creating buffer 3: " << err << std::endl;
        return 1;
    }
    Scope_exit release_mem_index([mem_index] {
        cl_int err2 = clReleaseMemObject(mem_index);
        if (err2 != CL_SUCCESS)
            std::cerr << "Error releasing buffer 2: " << err2 << std::endl;
    });

    Work work;
    while (std::cin >> work) {
        uint32_t found = 0, index = std::numeric_limits<uint32_t>::max();

        err = clEnqueueWriteBuffer(cmdqueue, mem_work,
                CL_TRUE, 0, sizeof(Work), &work,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error writing to buffer 0: " << err << std::endl;
            return 1;
        }

        err = clEnqueueWriteBuffer(cmdqueue, mem_found,
                CL_TRUE, 0, sizeof(uint32_t), &found,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error writing to buffer 1: " << err << std::endl;
            return 1;
        }

        err = clEnqueueWriteBuffer(cmdqueue, mem_index,
                CL_TRUE, 0, sizeof(uint32_t), &index,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error writing to buffer 2: " << err << std::endl;
            return 1;
        }

        err = clSetKernelArg(kernel_md5rush, 0, sizeof(cl_mem), &mem_work);
        if (err != CL_SUCCESS) {
            std::cerr << "Error setting argument 0: " << err << std::endl;
            return 1;
        }

        err = clSetKernelArg(kernel_md5rush, 1, sizeof(cl_mem), &mem_found);
        if (err != CL_SUCCESS) {
            std::cerr << "Error setting argument 1: " << err << std::endl;
            return 1;
        }
        
        err = clSetKernelArg(kernel_md5rush, 2, sizeof(cl_mem), &mem_index);
        if (err != CL_SUCCESS) {
            std::cerr << "Error setting argument 2: " << err << std::endl;
            return 1;
        }

        uint64_t count = std::min(work.count, 0x100000000u);
        err = clEnqueueNDRangeKernel(cmdqueue, kernel_md5rush, 1,
                nullptr, &count, nullptr,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error executing kernel: " << err << std::endl;
            return 1;
        }

        err = clEnqueueReadBuffer(cmdqueue, mem_found,
                CL_TRUE, 0, sizeof(uint32_t), &found,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error reading buffer 1: " << err << std::endl;
            return 1;
        }

        err = clEnqueueReadBuffer(cmdqueue, mem_index,
                CL_TRUE, 0, sizeof(uint32_t), &index,
                0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Error reading buffer 2: " << err << std::endl;
            return 1;
        }

        if (found) {
            uint32_t result = work.data[work.mutable_index] + index;
            std::cout << "1 " << result << std::endl;
        } else {
            std::cout << "0 0" << std::endl;
        }
    }
}

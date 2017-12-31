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

bool check_cl_error(cl_int err) {
    if (err != CL_SUCCESS) {
        std::cerr << "OpenCL error: " << err << std::endl;
        return true;
    }
    return false;
}

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

std::optional<cl_platform_id> select_platform() {
    cl_uint size;
    if (check_cl_error(clGetPlatformIDs(0, nullptr, &size)))
        return std::nullopt;
    if (size == 0)
        return std::nullopt;

    std::vector<cl_platform_id> platforms(size);
    if (check_cl_error(clGetPlatformIDs(size, platforms.data(), nullptr)))
        return std::nullopt;

    // TODO proper selection
    return platforms[0];
}

std::optional<cl_device_id> select_device(cl_platform_id platform) {
    cl_uint size;
    if (check_cl_error(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0,
                nullptr, &size)))
        return std::nullopt;
    if (size == 0)
        return std::nullopt;

    std::vector<cl_device_id> devices(size);
    if (check_cl_error(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, size,
                devices.data(), nullptr)))
        return std::nullopt;

    // TODO proper selection
    return devices[0];
}

cl_context create_context(cl_platform_id platform, cl_device_id device) {
    cl_context_properties properties[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
        0,
    };
    cl_int err;
    cl_context context = clCreateContext(properties, 1, &device,
            nullptr, nullptr, &err);
    check_cl_error(err);
    return context;
}

cl_command_queue create_command_queue(
        cl_context context, cl_device_id device) {
    cl_int err;
    cl_command_queue cmdqueue = clCreateCommandQueue(context, device, 0, &err);
    check_cl_error(err);
    return cmdqueue;
}

cl_program create_program(cl_context context, const char *source) {
    cl_int err;
    cl_program program = clCreateProgramWithSource(
            context, 1, &source, nullptr, &err);
    check_cl_error(err);
    return program;
}

cl_kernel create_kernel(cl_program program, const char *kernel_name) {
    cl_int err;
    cl_kernel kernel = clCreateKernel(program, kernel_name, &err);
    check_cl_error(err);
    return kernel;
}

struct My_context {
    cl_context context;
    cl_kernel kernel_md5rush, kernel_ffz;
    cl_command_queue cmdqueue;
    size_t ffz_work_group_size;
};

struct Work {
    std::array<uint32_t, 4> init_state;
    std::array<uint32_t, 4> mask;
    std::array<uint32_t, 16> data;
    uint32_t mutable_index;
    uint64_t count;
    Work() = default;
};

std::istream &operator >> (std::istream &in, Work &work) {
    for (uint32_t &u : work.init_state)
        in >> u;
    for (uint32_t &u : work.mask)
        in >> u;
    for (uint32_t &u : work.data)
        in >> u;
    return in >> work.mutable_index >> work.count;
}

constexpr const char *md5rush_source = R"(
__constant uint s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

__constant uint k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

struct Work {
    uint init_state[4];
    uint mask[4];
    uint data[16];
    uint mutable_index;
    ulong count; // unused
};

__kernel void md5rush(__constant struct Work *work, __global uint *result) {
    uint a = work->init_state[0];
    uint b = work->init_state[1];
    uint c = work->init_state[2];
    uint d = work->init_state[3];
#define MD5_STATE_UPDATE_LOOP(IBEGIN, IEND, FEXPR, GEXPR) \
    for (uint i = (IBEGIN); i < (IEND); i++) { \
        uint f = (FEXPR) + a + k[i] + work->data[(GEXPR)] + \
            ((GEXPR) == work->mutable_index ? get_global_id(0) : 0); \
        a = d; \
        d = c; \
        c = b; \
        b += (f << s[i]) | (f >> (32 - s[i])); \
    }
#pragma unroll
    MD5_STATE_UPDATE_LOOP( 0, 16, (b & c) | (~b & d),      i          )
#pragma unroll
    MD5_STATE_UPDATE_LOOP(16, 32, (d & b) | (~d & c), (5 * i + 1) % 16)
#pragma unroll
    MD5_STATE_UPDATE_LOOP(32, 48, b ^ c ^ d         , (3 * i + 5) % 16)
#pragma unroll
    MD5_STATE_UPDATE_LOOP(48, 64, c ^ (b | ~d)      ,  7 * i      % 16)
#undef MD5_STATE_UPDATE_LOOP
    a += work->init_state[0];
    b += work->init_state[1];
    c += work->init_state[2];
    d += work->init_state[3];
    a &= work->mask[0];
    b &= work->mask[1];
    c &= work->mask[2];
    d &= work->mask[3];
    result[get_global_id(0)] = a | b | c | d;
}
__kernel void find_first_zero(__global uint *a, ulong size,
        __global ulong *result) {
    ulong ans = size;
    for (ulong i = get_global_id(0); i < size; i += get_global_size(0)) {
        ulong newans = a[i] ? size : i;
        ans = ans < newans ? ans : newans;
    }
    result[get_global_id(0)] = ans;
}
)";

std::optional<uint32_t> md5rush(const Work &work, const My_context &context) {
    // Good luck pwning me.
    if (work.mutable_index >= work.data.size())
        return std::nullopt;
    // Trying duplicate messages is a waste.
    uint64_t count = std::min(work.count, 0x100000000u);
    size_t ffz_count = context.ffz_work_group_size;

    cl_int err;
    cl_mem mem_work = clCreateBuffer(context.context,
            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY,
            sizeof(Work), (void*) &work, &err);
    if (check_cl_error(err))
        return std::nullopt;
    Scope_exit release_mem_work([mem_work] () {
        check_cl_error(clReleaseMemObject(mem_work));
    });

    cl_mem mem_temp = clCreateBuffer(context.context,
            CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS,
            count * 4, nullptr, &err);
    if (check_cl_error(err))
        return std::nullopt;
    Scope_exit release_mem_temp([mem_temp] () {
        check_cl_error(clReleaseMemObject(mem_temp));
    });

    cl_mem mem_result = clCreateBuffer(context.context,
            CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
            8 * ffz_count, nullptr, &err);
    if (check_cl_error(err))
        return std::nullopt;
    Scope_exit release_mem_result([mem_result] () {
        check_cl_error(clReleaseMemObject(mem_result));
    });

    if (check_cl_error(clSetKernelArg(
            context.kernel_md5rush, 0, sizeof(cl_mem), &mem_work)))
        return std::nullopt;
    if (check_cl_error(clSetKernelArg(
            context.kernel_md5rush, 1, sizeof(cl_mem), &mem_temp)))
        return std::nullopt;
    if (check_cl_error(clEnqueueNDRangeKernel(
            context.cmdqueue, context.kernel_md5rush, 1,
            nullptr, &count, nullptr, 0, nullptr, nullptr)))
        return std::nullopt;

    if (check_cl_error(clSetKernelArg(
            context.kernel_ffz, 0, sizeof(cl_mem), &mem_temp)))
        return std::nullopt;
    if (check_cl_error(clSetKernelArg(
            context.kernel_ffz, 1, 8, &count)))
        return std::nullopt;
    if (check_cl_error(clSetKernelArg(
            context.kernel_ffz, 2, sizeof(cl_mem), &mem_result)))
        return std::nullopt;
    if (check_cl_error(clEnqueueNDRangeKernel(
            context.cmdqueue, context.kernel_ffz, 1,
            nullptr, &ffz_count, nullptr, 0, nullptr, nullptr)))
        return std::nullopt;

    std::vector<uint64_t> vec_result(ffz_count, count);
    if (check_cl_error(clEnqueueReadBuffer(
            context.cmdqueue, mem_result, CL_TRUE, 0, 8 * ffz_count,
            vec_result.data(), 0, nullptr, nullptr)))
        return std::nullopt;

    uint64_t result = *std::min_element(vec_result.begin(), vec_result.end());
    if (result >= count)
        return std::nullopt;

    return work.data[work.mutable_index] + result;
}

}

int main() {
    std::optional<cl_platform_id> platform = select_platform();
    if (!platform) {
        std::cerr << "No platform found." << std::endl;
        return 1;
    }

    std::optional<cl_device_id> device = select_device(*platform);
    if (!device) {
        std::cerr << "No device found." << std::endl;
        return 1;
    }

    cl_context context = create_context(*platform, *device);
    if (!context) {
        std::cerr << "Failed to create context." << std::endl;
        return 1;
    }
    Scope_exit release_context([context] () {
        check_cl_error(clReleaseContext(context));
    });

    cl_program program = create_program(context, md5rush_source);
    if (!program) {
        std::cerr << "Failed to create program." << std::endl;
        return 1;
    }
    Scope_exit release_program([program] () {
        check_cl_error(clReleaseProgram(program));
    });

    if (check_cl_error(clBuildProgram(
                    program, 1, &*device, "", nullptr, nullptr))) {
        std::cerr << "Failed to build program." << std::endl;
        // Determine the size of the log
        size_t log_size;
        if (check_cl_error(clGetProgramBuildInfo(program, *device,
                    CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size))) {
            std::cerr << "Failed to get build log." << std::endl;
            return 1;
        }
        std::vector<char> log(log_size + 1); 
        if (check_cl_error(clGetProgramBuildInfo(program, *device,
                    CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr))) {
            std::cerr << "Failed to get build log." << std::endl;
            return 1;
        }
        std::cerr << log.data() << std::endl;
        return 1;
    }

    cl_kernel kernel_md5rush = create_kernel(program, "md5rush");
    if (!kernel_md5rush) {
        std::cerr << "Failed to create kernel \"md5rush\"." << std::endl;
        return 1;
    }
    Scope_exit release_kernel_md5rush([kernel_md5rush] () {
        check_cl_error(clReleaseKernel(kernel_md5rush));
    });

    cl_kernel kernel_ffz = create_kernel(program, "find_first_zero");
    if (!kernel_ffz) {
        std::cerr << "Failed to create kernel \"find_first_zero\"."
            << std::endl;
        return 1;
    }
    Scope_exit release_kernel_ffz([kernel_ffz] () {
        check_cl_error(clReleaseKernel(kernel_ffz));
    });

    size_t ffz_work_group_size = 0;
    if (check_cl_error(clGetKernelWorkGroupInfo(kernel_ffz, *device,
            CL_KERNEL_WORK_GROUP_SIZE, sizeof(ffz_work_group_size),
            &ffz_work_group_size, nullptr))) {
        std::cerr << "Failed to get CL_KERNEL_WORK_GROUP_SIZE"
            " of \"find_first_zero\"." << std::endl;
        return 1;
    }

    cl_command_queue cmdqueue = create_command_queue(context, *device);
    if (!cmdqueue) {
        std::cerr << "Failed to create command queue." << std::endl;
        return 1;
    }
    Scope_exit release_cmdqueue([cmdqueue] () {
        check_cl_error(clReleaseCommandQueue(cmdqueue));
    });

    struct Work work;
    struct My_context my_context = {
        context,
        kernel_md5rush, kernel_ffz,
        cmdqueue,
        ffz_work_group_size,
    };
    while (std::cin >> work) {
        std::optional<uint32_t> result = md5rush(work, my_context);
        if (result) {
            std::cout << "1 " << *result << std::endl;
        } else {
            std::cout << "0 0" << std::endl;
        }
    }
}

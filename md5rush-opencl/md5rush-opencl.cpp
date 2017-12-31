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

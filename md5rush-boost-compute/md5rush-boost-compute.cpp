#include <algorithm>
#include <iostream>
#include <iterator>
#include <limits>

#include <boost/compute/core.hpp>

namespace {
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
    auto device = boost::compute::system::default_device();

    boost::compute::context context(device);

    auto program = boost::compute::program::create_with_source(
            md5rush_source, context);

    try {
        program.build();
    } catch (boost::compute::opencl_error &clerror) {
        std::cerr << program.get_build_info<std::string>(
                CL_PROGRAM_BUILD_LOG, device) << std::endl;
        return 1;
    }

    boost::compute::kernel kernel_md5rush(program, "md5rush");

    boost::compute::command_queue cmdqueue(context, device);

    boost::compute::buffer mem_work(context, sizeof(Work),
            CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY);
    boost::compute::buffer mem_found(context, sizeof(uint32_t));
    boost::compute::buffer mem_index(context, sizeof(uint32_t));

    struct Work work;
    while (std::cin >> work) {
        uint32_t found = 0, index = std::numeric_limits<uint32_t>::max();
        cmdqueue.enqueue_write_buffer(mem_work, 0, sizeof(Work), &work);
        cmdqueue.enqueue_write_buffer(mem_found, 0, sizeof(uint32_t), &found);
        cmdqueue.enqueue_write_buffer(mem_index, 0, sizeof(uint32_t), &index);

        kernel_md5rush.set_arg(0, mem_work);
        kernel_md5rush.set_arg(1, mem_found);
        kernel_md5rush.set_arg(2, mem_index);

        uint64_t count = std::min(work.count, 0x100000000u);
        cmdqueue.enqueue_1d_range_kernel(kernel_md5rush, 0, count, 0);

        cmdqueue.enqueue_read_buffer(mem_found, 0, sizeof(uint32_t), &found);
        cmdqueue.enqueue_read_buffer(mem_index, 0, sizeof(uint32_t), &index);

        if (found) {
            uint32_t result = work.data[work.mutable_index] + index;
            std::cout << "1 " << result << std::endl;
        } else {
            std::cout << "0 0" << std::endl;
        }
    }
}

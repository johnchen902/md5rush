#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>

#ifndef MD5RUSH_VECTOR_WIDTH
#if __AVX512F__
#define MD5RUSH_VECTOR_WIDTH 16
#elif __AVX2__
#define MD5RUSH_VECTOR_WIDTH 8
#elif __SSE2__
#define MD5RUSH_VECTOR_WIDTH 4
#elif __MMX__
#define MD5RUSH_VECTOR_WIDTH 2
#else
#define MD5RUSH_VECTOR_WIDTH 1
#endif
#endif

namespace {

constexpr unsigned vector_width = MD5RUSH_VECTOR_WIDTH;

using vector_t =
    uint32_t __attribute__((vector_size(sizeof(uint32_t) * vector_width)));

bool may_have_zero(vector_t x) {
#if MD5RUSH_VECTOR_WIDTH == 16
#error "may_have_zero unimplemented for vector_width == 16"
#elif MD5RUSH_VECTOR_WIDTH == 8
    return __builtin_ia32_movmskps256(x == 0);
#elif MD5RUSH_VECTOR_WIDTH == 4
    return __builtin_ia32_movmskps(x == 0);
#else
    return true;
#endif
}

constexpr uint32_t s[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

constexpr uint32_t k[64] = {
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

std::array<vector_t, 4> next_state(const std::array<vector_t, 4> &state,
        const std::array<vector_t, 16> &m) {
    auto [a, b, c, d] = state;
#define MD5_STATE_UPDATE_LOOP(IBEGIN, IEND, FEXPR, GEXPR) \
    for (uint32_t i = (IBEGIN); i < (IEND); i++) { \
        vector_t f = (FEXPR); \
        uint32_t g = (GEXPR); \
        f += a + k[i] + m[g]; \
        a = d; \
        d = c; \
        c = b; \
        b += (f << s[i]) | (f >> (32 - s[i])); \
    }

    MD5_STATE_UPDATE_LOOP( 0, 16, (b & c) | (~b & d),      i          )
    MD5_STATE_UPDATE_LOOP(16, 32, (d & b) | (~d & c), (5 * i + 1) % 16)
    MD5_STATE_UPDATE_LOOP(32, 48, b ^ c ^ d         , (3 * i + 5) % 16)
    MD5_STATE_UPDATE_LOOP(48, 64, c ^ (b | ~d)      ,  7 * i      % 16)
#undef MD5_STATE_UPDATE_LOOP

    return {state[0] + a, state[1] + b, state[2] + c, state[3] + d};
}

struct Work {
    std::array<uint32_t, 4> init_state;
    std::array<uint32_t, 4> mask;
    std::array<uint32_t, 16> data;
    unsigned mutable_index;
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

template<size_t n>
std::array<vector_t, n> broadcast(const std::array<uint32_t, n> &in) {
    std::array<vector_t, n> out;
    for (size_t i = 0; i < n; i++)
        out[i] = vector_t{} + in[i];
    return out;
}

std::optional<uint32_t> md5rush(const Work &work) {
    // Good luck pwning me.
    if (work.mutable_index >= work.data.size())
        return std::nullopt;
    // Trying duplicate messages is a waste.
    uint64_t count = std::min(work.count, 0x100000000u);

    std::array<vector_t, 4> init_state = broadcast(work.init_state);
    std::array<vector_t, 4> mask = broadcast(work.mask);
    std::array<vector_t, 16> data = broadcast(work.data);
    for (unsigned j = 0; j < vector_width; j++)
        data[work.mutable_index][j] += j;

    for (uint64_t i = 0; i < count; i += vector_width) {
        std::array<vector_t, 4> new_state = next_state(init_state, data);
        vector_t masked_state =
            (new_state[0] & mask[0]) |
            (new_state[1] & mask[1]) |
            (new_state[2] & mask[2]) |
            (new_state[3] & mask[3]);
        if (may_have_zero(masked_state))
            for (unsigned j = 0; j < vector_width; j++)
                if (masked_state[j] == 0)
                    return work.data[work.mutable_index] + i + j;
        data[work.mutable_index] += vector_width;
    }
    return std::nullopt;
}

}

int main() {
    struct Work work;
    while (std::cin >> work) {
        std::optional<uint32_t> result = md5rush(work);
        if (result) {
            std::cout << "1 " << *result << std::endl;
        } else {
            std::cout << "0 0" << std::endl;
        }
    }
}

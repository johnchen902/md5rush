#include "md5.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#ifdef MD5_MAIN
#include <cstring>
#endif

namespace md5 {
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

    constexpr uint32_t bit[32] = {
        1u <<  7, 1 <<  6, 1 <<  5, 1 <<  4, 1 <<  3, 1 <<  2, 1 <<  1, 1 <<  0,
        1u << 15, 1 << 14, 1 << 13, 1 << 12, 1 << 11, 1 << 10, 1 <<  9, 1 <<  8,
        1u << 23, 1 << 22, 1 << 21, 1 << 20, 1 << 19, 1 << 18, 1 << 17, 1 << 16,
        1u << 31, 1 << 30, 1 << 29, 1 << 28, 1 << 27, 1 << 26, 1 << 25, 1 << 24,
    };

    constexpr State update(State state, const uint32_t *m) {
        auto [a, b, c, d] = state;
#define MD5_STATE_UPDATE_LOOP(IBEGIN, IEND, FEXPR, GEXPR) \
        for (uint32_t i = (IBEGIN); i < (IEND); i++) { \
            uint32_t f = (FEXPR), g = (GEXPR); \
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

        state.a += a;
        state.b += b;
        state.c += c;
        state.d += d;
        return state;
    }

    constexpr State md5(const uint32_t *d, size_t nbits) {
        State state;
        size_t rem = nbits;
        while (rem >= 512) {
            state = update(state, d);
            d += 16;
            rem -= 512;
        }

        uint32_t buf[32] = {};
        for (uint32_t i = 0; i < rem / 32; i++)
            buf[i] = d[i];
        for (uint32_t i = 0; i < rem % 32; i++)
            buf[rem / 32] |= d[rem / 32] & bit[i];
        buf[rem / 32] |= bit[rem % 32];
        if (rem + 1 + 64 <= 512) {
            // one block left
            buf[14] = nbits;
            buf[15] = nbits >> 32;
            state = update(state, buf);
        } else {
            // two blocks left
            buf[30] = nbits;
            buf[31] = nbits >> 32;
            state = update(state, buf);
            state = update(state, buf + 16);
        }
        return state;
    }

    std::ostream &operator << (std::ostream &out, State state) {
        std::ios::fmtflags flags(out.flags());
        out << std::right << std::hex << std::setfill('0')
             << std::setw(8) << __builtin_bswap32(state.a)
             << std::setw(8) << __builtin_bswap32(state.b)
             << std::setw(8) << __builtin_bswap32(state.c)
             << std::setw(8) << __builtin_bswap32(state.d);
        out.flags(flags);
        return out;
    }
}

#ifdef MD5_MAIN
int main(int argc, char **argv) {
    if (argc <= 1) {
        std::cerr << "Usage: " << argv[0] << " data..." << std::endl;
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        size_t length = std::strlen(argv[i]) * 8;
        uint32_t *d = reinterpret_cast<uint32_t *>(argv[i]);
        std::cout << md5::md5(d, length) << std::endl;
    }
}
#endif

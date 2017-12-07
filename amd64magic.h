#ifndef MD5RUSH_AMD64MAGIC_H
#define MD5RUSH_AMD64MAGIC_H
#include <cstdint>
#include <x86intrin.h>

namespace amd64magic {
    using v4uint32_t =
        uint32_t __attribute__((vector_size(sizeof(uint32_t) * 4)));
    using v8uint32_t =
        uint32_t __attribute__((vector_size(sizeof(uint32_t) * 8)));
    using v16uint32_t =
        uint32_t __attribute__((vector_size(sizeof(uint32_t) * 16)));

#ifdef MD5RUSH_VFASTUINT32
    using vfastuint32_t = MD5_VFASTUINT32_T;
#elif __AVX512F__
    using vfastuint32_t = v16uint32_t;
#elif __AVX2__
    using vfastuint32_t = v8uint32_t;
#elif __SSE2__
    using vfastuint32_t = v4uint32_t;
#else
    using vfastuint32_t = uint32_t;
#endif

    template<typename T>
    constexpr size_t width = sizeof(T) / sizeof(uint32_t);

    template<typename T>
    constexpr bool is_all_zero(T x) {
        return !x;
    }

    template<>
    inline bool is_all_zero<v4uint32_t>(v4uint32_t x) {
#if __SSE4_1__
        return _mm_testz_si128((__m128i) x, (__m128i) x);
#else
        // https://stackoverflow.com/a/10250306
        return _mm_movemask_epi8(_mm_cmpeq_epi8(
                    (__m128i) x, _mm_setzero_si128())) == 0xFFFF;
#endif
    }

    template<>
    inline bool is_all_zero<v8uint32_t>(v8uint32_t x) {
        return _mm256_testz_si256((__m256i) x, (__m256i) x);
    }

#if 0
    template<>
    inline bool is_all_zero<v16uint32_t>(v16uint32_t x) {
        // TODO implement
    }
#endif
}
#endif // MD5RUSH_AMD64MAGIC_H

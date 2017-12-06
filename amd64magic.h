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
    constexpr bool vector_any(T x);

    template<>
    constexpr bool vector_any<uint32_t>(uint32_t x) { return x; }

    template<>
    inline bool vector_any<v8uint32_t>(v8uint32_t x) {
        return !_mm256_testz_si256((__m256i) x, (__m256i) x);
    }

    // TODO implement vector_any for v4uint32_t and v16uint32_t
}
#endif // MD5RUSH_AMD64MAGIC_H

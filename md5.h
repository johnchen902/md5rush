#ifndef MD5RUSH_MD5_H
#define MD5RUSH_MD5_H
#include <cstdint>
#include <ostream>
#include "amd64magic.h"

namespace md5 {
    template<typename Vector_type>
    struct Vector_state {
        using vector_type = Vector_type;
        vector_type a, b, c, d;
        constexpr Vector_state(vector_type _a, vector_type _b,
                vector_type _c, vector_type _d):
            a(_a), b(_b), c(_c), d(_d) {}
        constexpr Vector_state(uint32_t _a, uint32_t _b,
                uint32_t _c, uint32_t _d):
            a(vector_type{} + _a), b(vector_type{} + _b),
            c(vector_type{} + _c), d(vector_type{} + _d) {}
        constexpr Vector_state():
            Vector_state(0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476) {}
        constexpr Vector_state(Vector_state<uint32_t> state);
        constexpr Vector_state<uint32_t> operator[] (unsigned i);
    };

    template<>
    struct Vector_state<uint32_t> {
        using vector_type = uint32_t;
        uint32_t a, b, c, d;
        constexpr Vector_state(uint32_t _a, uint32_t _b,
                uint32_t _c, uint32_t _d): a(_a), b(_b), c(_c), d(_d) {}
        constexpr Vector_state():
            Vector_state(0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476) {}
        constexpr Vector_state<uint32_t> operator[] (unsigned) { return *this; }
    };

    template<typename Vector_type>
    constexpr Vector_state<Vector_type>::Vector_state(Vector_state<uint32_t> state):
        Vector_state(state.a, state.b, state.c, state.d) {}

    template<typename Vector_type>
    constexpr Vector_state<uint32_t>
    Vector_state<Vector_type>::operator[] (unsigned i) {
        return Vector_state<uint32_t>(a[i], b[i], c[i], d[i]);
    }

    using State = Vector_state<uint32_t>;
    using State_v4 = Vector_state<amd64magic::v4uint32_t>;
    using State_v8 = Vector_state<amd64magic::v8uint32_t>;
    using State_v16 = Vector_state<amd64magic::v16uint32_t>;
    using State_vfast = Vector_state<amd64magic::vfastuint32_t>;

    State update(State state, const uint32_t *m)
        __attribute__((warn_unused_result));
    State_v4 update(State_v4 state, const amd64magic::v4uint32_t *m)
        __attribute__((warn_unused_result));
    State_v8 update(State_v8 state, const amd64magic::v8uint32_t *m)
        __attribute__((warn_unused_result));
    State_v16 update(State_v16 state, const amd64magic::v16uint32_t *m)
        __attribute__((warn_unused_result));

    State md5(const uint32_t *d, size_t nbits);

    std::ostream &operator << (std::ostream &out, State state);
}
#endif // MD5RUSH_MD5_H

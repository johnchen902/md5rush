#include <cstdint>
#include <iostream>

namespace md5 {
    struct State {
        uint32_t a, b, c, d;
        constexpr State(uint32_t _a, uint32_t _b, uint32_t _c, uint32_t _d):
            a(_a), b(_b), c(_c), d(_d) {}
        constexpr State():
            State(0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476) {}
    };

    constexpr State update(State state, const uint32_t *m)
        __attribute__((warn_unused_result));

    constexpr State md5(const uint32_t *d, size_t nbits);

    std::ostream &operator << (std::ostream &out, State state);
}

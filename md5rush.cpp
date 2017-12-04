#include <cstdio>
#include <cstring>
#include <algorithm>
#include <openssl/md5.h>
#include <limits>

template<typename Predicate>
size_t find_next_treasure(unsigned char *result, size_t prefix_length,
                          size_t maxlen, Predicate pred) {
    size_t length = prefix_length;
    while (!pred(result, length)) {
        for (size_t i = length; ; i--) {
            if (i == prefix_length) {
                if (length == maxlen)
                    return maxlen + 1;
                result[length++] = 0;
                break;
            }
            if (result[i - 1] < std::numeric_limits<unsigned char>::max()) {
                result[i - 1]++;
                break;
            }
            result[i - 1] = 0;
        }
    }
    return length;
}

struct MD5_zeroes {
    size_t zeroes;
    bool operator () (const unsigned char *d, size_t n) const {
        unsigned char md[MD5_DIGEST_LENGTH];
        MD5(d, n, md);
        for (size_t i = 0; i < zeroes / 2; i++)
            if (md[i])
                return false;
        if ((zeroes & 1) && (md[zeroes / 2] & 0xf0))
            return false;
        return true;
    }
};

int main() {
    constexpr const char *init_prefix = "B04902114";
    constexpr size_t init_prefix_len = std::strlen(init_prefix);
    constexpr size_t maxlen = 128;
    
    unsigned char prefix[maxlen];
    size_t prefix_length = init_prefix_len;
    std::copy_n(init_prefix, init_prefix_len, prefix);

    for (size_t zeroes = 1; zeroes <= 32; zeroes++) {
        size_t result_length = find_next_treasure(prefix,
                prefix_length, maxlen, MD5_zeroes{zeroes});

        if (result_length > maxlen) {
            std::fprintf(stderr, "search space exhausted\n");
            return 1;
        }

        char filename[128];
        std::sprintf(filename, "treasure-%zd", zeroes);
        if (FILE *f = std::fopen(filename, "w")) {
            std::fwrite(prefix, result_length, 1, f);
            std::fclose(f);
        }

        for (size_t i = 0; i < result_length; i++)
            std::printf("%02x", prefix[i]);
        std::putchar('\n');
        std::fflush(stdout);

        unsigned char md[MD5_DIGEST_LENGTH];
        MD5(prefix, result_length, md);

        for (size_t i = 0; i < MD5_DIGEST_LENGTH; i++)
            std::printf("%02x", md[i]);
        std::putchar('\n');
        std::fflush(stdout);

        prefix_length = result_length;
    }
}

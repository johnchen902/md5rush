#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>
#include <openssl/md5.h>
#include <thread>
#include <vector>

#include "md5.h"

/**
 * Increase the sequence [first, last) by step.
 * The sequence cannot expand beyond cap.
 *
 * Returns the new last if ok, otherwise return nullptr.
 */
unsigned char *next_seq(unsigned char *first, unsigned char *last,
        unsigned char *cap, unsigned step) {
    using T = std::remove_reference<decltype(*first)>::type;
    static_assert(std::numeric_limits<T>::is_specialized);

    auto carry = step;
    for (; carry && first != last; ++first) {
        carry += *first;
        *first = carry;
        carry /= std::numeric_limits<T>::max() + 1;
    }
    for (; carry && last != cap; ++first, ++last) {
        carry--;
        *first = carry;
        carry /= std::numeric_limits<T>::max() + 2;
    }
    return carry ? nullptr : last;
}

template<typename Predicate>
unsigned char *find_next_treasure(unsigned char *first,
        unsigned char *prefix_last, unsigned char *last, unsigned char *cap,
        unsigned step, Predicate pred) {
    while (last && !pred(first, last))
        last = next_seq(prefix_last, last, cap, step);
    return last;
}

template<typename Predicate>
class Stop_predicate {
    Predicate pred;
    std::atomic<bool> &stop;
public:
    Stop_predicate(Predicate _pred, std::atomic<bool> &_stop):
            pred(_pred), stop(_stop) {}
    template<typename ...Params>
    bool operator () (Params &&...args) const {
        return stop || pred(std::forward<Params>(args)...);
    }
};

template<typename Predicate>
void find_next_treasure_inthread(unsigned char *first,
        unsigned char *prefix_last, unsigned char *last, unsigned char *cap,
        unsigned step, Predicate pred, std::atomic<bool> &stop,
        unsigned char *&result) {
    unsigned char *result0 = find_next_treasure(first,
            prefix_last, last, cap, step, Stop_predicate(pred, stop));
    if (result0 && !stop.exchange(true, std::memory_order_relaxed))
        result = result0;
    else
        result = nullptr;
}

template<typename Predicate>
unsigned char *find_next_treasure_multithread(unsigned char *first,
        unsigned char *prefix_last, unsigned char *last, unsigned char *cap,
        unsigned nthreads, Predicate pred) {
    std::vector<std::vector<unsigned char>> buffers;
    std::vector<unsigned char*> results;
    std::vector<std::thread> threads;

    buffers.reserve(nthreads);
    threads.reserve(nthreads);
    results.reserve(nthreads);
    std::atomic<bool> stop = false;

    for (size_t i = 0; last && i < nthreads; i++) {
        buffers.emplace_back(cap - first);
        unsigned char *tfirst = buffers[i].data();
        unsigned char *tprefix_last = tfirst + (prefix_last - first);
        unsigned char *tlast = tfirst + (last - first);
        unsigned char *tcap = tfirst + (cap - first);
        std::copy(first, last, tfirst);

        results.emplace_back(nullptr);

        threads.emplace_back(find_next_treasure_inthread<Predicate>,
            tfirst, tprefix_last, tlast, tcap,
            nthreads, pred, std::ref(stop), std::ref(results[i]));

        last = next_seq(prefix_last, last, cap, 1);
    }

    for (std::thread &t : threads)
        t.join();

    for (size_t i = 0; i < threads.size(); i++) {
        if (results[i]) {
            unsigned char *tfirst = buffers[i].data();
            unsigned char *tlast = results[i];
            return std::copy(tfirst, tlast, first);
        }
    }
    return nullptr;
}

class MD5_zeroes {
    size_t zeroes;
public:
    MD5_zeroes(size_t _zeroes) : zeroes(_zeroes) {}
    bool operator () (const unsigned char *first, const unsigned char *last) const {
        unsigned char md[MD5_DIGEST_LENGTH];
        MD5(first, last - first, md);
        for (size_t i = 0; i < zeroes / 2; i++)
            if (md[i])
                return false;
        if ((zeroes & 1) && (md[zeroes / 2] & 0xf0))
            return false;
        return true;
    }
};

constexpr const char *init_prefix = "B04902114";
constexpr size_t init_prefix_len = std::strlen(init_prefix);
constexpr size_t maxlen = 128;

int main() {
    unsigned nthreads = std::thread::hardware_concurrency();
    if (nthreads == 0) {
        std::fputs("std::thread::hardware_concurrency() returned 0", stderr);
        return 1;
    }
    std::fprintf(stderr, "Using %u threads.\n", nthreads);

    unsigned char first[maxlen];
    unsigned char *prefix_last =
            std::copy_n(init_prefix, init_prefix_len, first);
    unsigned char *const cap = std::end(first);

    for (size_t zeroes = 1; zeroes <= 32; zeroes++) {
        unsigned char *last = find_next_treasure_multithread(first,
                prefix_last, prefix_last, cap, nthreads,
                MD5_zeroes(zeroes));

        if (!last) {
            std::fprintf(stderr, "search space exhausted\n");
            return 1;
        }

        char filename[128];
        std::sprintf(filename, "treasure-%zd", zeroes);
        if (FILE *f = std::fopen(filename, "w")) {
            std::fwrite(first, last - first, 1, f);
            std::fclose(f);
        }

        for (unsigned char *p = first; p != last; ++p)
            std::printf("%02x", *p);
        std::putchar('\n');
        std::fflush(stdout);

        unsigned char md[MD5_DIGEST_LENGTH];
        MD5(first, last - first, md);

        for (unsigned char ch : md)
            std::printf("%02x", ch);
        std::putchar('\n');
        std::fflush(stdout);

        prefix_last = last;
    }
}

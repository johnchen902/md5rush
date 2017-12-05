#include <algorithm>
#include <atomic>
#include <cassert>
#include <iterator>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h> // getopt
#include <vector>

#include "md5.h"

/**
 * Increase the sequence [first, last) by addend.
 *
 * Returns carry (when the sequence is full).
 */
template<typename Iterator>
auto next_sequence(Iterator first, Iterator last,
        typename std::iterator_traits<Iterator>::value_type addend) {
    for (; addend && first != last; ++first)
        addend = __builtin_add_overflow(*first, addend, std::addressof(*first));
    return addend;
}

/**
 * Find 'next' treasure satisfying pred(begin, end).
 * 'Next' means next_sequence(mutable_begin, mutable_end, step_size).
 *
 * Returns true if one is found, false otherwise.
 */
template<typename Iterator, typename Predicate>
bool next_treasure(Iterator begin, Iterator end,
        Iterator mutable_begin, Iterator mutable_end,
        typename std::iterator_traits<Iterator>::value_type step_size,
        Predicate pred) {
    do {
        if (pred(begin, end))
            return true;
    } while (!next_sequence(mutable_begin, mutable_end, step_size));
    return false;
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
        return stop.load(std::memory_order_relaxed) ||
            pred(std::forward<Params>(args)...);
    }
};

template<typename Iterator, typename Predicate>
void next_treasure_for_thread(Iterator begin, Iterator end,
        Iterator mutable_begin, Iterator mutable_end,
        typename std::iterator_traits<Iterator>::value_type step_size,
        Predicate pred, std::atomic<bool> &stop, bool &return_value) {
    return_value = next_treasure(begin, end, mutable_begin, mutable_end,
            step_size, Stop_predicate(pred, stop));
    if (stop.load(std::memory_order_relaxed))
        return_value = false;
    if (return_value)
        stop.store(true, std::memory_order_relaxed);
}

template<typename Iterator, typename Predicate>
bool next_treasure_multithread(Iterator begin, Iterator end,
        Iterator mutable_begin, Iterator mutable_end,
        typename std::iterator_traits<Iterator>::value_type step_size,
        Predicate pred, unsigned nthreads) {
    using T = typename std::iterator_traits<Iterator>::value_type;
    std::vector<std::vector<T>> buffers;
    std::vector<std::array<bool, 1>> results;
    std::vector<std::thread> threads;

    buffers.reserve(nthreads);
    threads.reserve(nthreads);
    results.reserve(nthreads);
    std::atomic<bool> stop = false;

    for (size_t i = 0; i < nthreads; i++) {
        buffers.emplace_back(begin, end);
        results.emplace_back();

        auto tbegin = buffers[i].data();
        auto tend = tbegin + (end - begin);
        auto tmutable_begin = tbegin + (mutable_begin - begin);
        auto tmutable_end = tbegin + (mutable_end - begin);
        threads.emplace_back(next_treasure_for_thread<Iterator, Predicate>,
            tbegin, tend, tmutable_begin, tmutable_end,
            step_size * nthreads, // TODO overflow check
            pred, std::ref(stop), std::ref(results[i][0]));

        if (next_sequence(mutable_begin, mutable_end, step_size))
            break;
    }

    for (std::thread &t : threads)
        t.join();

    for (size_t i = 0; i < threads.size(); i++) {
        if (results[i][0]) {
            auto tbegin = buffers[i].data();
            auto tend = tbegin + (end - begin);
            std::copy(tbegin, tend, begin);
            return true;
        }
    }
    return false;
}

class MD5_zeroes {
    constexpr static uint32_t zero_masks[8] = {
        0x00000000,
        0x000000f0,
        0x000000ff,
        0x0000f0ff,
        0x0000ffff,
        0x00f0ffff,
        0x00ffffff,
        0xf0ffffff,
    };
    md5::State init_state;
    uint32_t zeroes;
public:
    MD5_zeroes(md5::State _init_state, uint32_t _zeroes) :
        init_state(_init_state), zeroes(_zeroes) {}
    bool operator () (const uint32_t *begin, const uint32_t *end) const {
        assert(end - begin == 16);

        md5::State state = md5::update(init_state, begin);

        if (zeroes < 8) {
            return (state.a & zero_masks[zeroes]) == 0;
        } else if (zeroes < 16) {
            if (state.a)
                return false;
            return (state.b & zero_masks[zeroes - 8]) == 0;
        } else if (zeroes < 24) {
            if (state.a || state.b)
                return false;
            return (state.c & zero_masks[zeroes - 16]) == 0;
        } else if (zeroes < 32) {
            if (state.a || state.b || state.c)
                return false;
            return (state.d & zero_masks[zeroes - 24]) == 0;
        } else { // zeroes == 32
            if (state.a || state.b || state.c || state.d)
                return false;
            return true;
        }
    }
};

void prepare_last_block(uint32_t *begin, uint32_t *mutable_begin,
        uint32_t *mutable_end, size_t nbits) {
    uint32_t *end = begin + 16;
    assert(begin <= mutable_begin);
    assert(mutable_begin <= mutable_end);
    assert(mutable_end + 3 <= end);

    std::fill(mutable_begin, mutable_end, 0);
    *mutable_end = 0x00000080;
    std::fill(mutable_end + 1, end - 2, 0);
    end[-2] = nbits;
    end[-1] = nbits >> 32;
}

std::vector<uint32_t> string_to_uint32(const std::string &str) {
    std::vector<uint32_t> v((str.size() + 3) / 4);
    for (size_t i = 0; i < str.size(); i++)
        v[i / 4] |= (unsigned char) str[i] << (i % 4 * 8);
    return v;
}

std::string uint32_to_string(const std::vector<uint32_t> &v) {
    std::string str(v.size() * 4, '\0');
    for (size_t i = 0; i < str.size(); i++)
        str[i] = (v[i / 4] >> (i % 4 * 8)) & 0xff;
    return str;
}

template<typename Iterator>
void print_treasure(std::ostream &out, Iterator begin, Iterator end) {
    std::ios::fmtflags flags(out.flags());
    out << std::right << std::hex << std::setfill('0');
    while (begin != end)
        out << std::setw(8) << __builtin_bswap32(*begin++);
    out.flags(flags);
}

void usage(const char *progname, std::ostream &out) {
    using std::endl;
    out << "Usage: " << progname << " [OPTION]... -z ZEROES PREFIXFILE\n"
        << "\n"
        << "  -z ZEROES      number of zeroes to look for\n"
        << "  -t THREADS     number of threads to use\n"
        << "                 (0: use std::thread::hardware_concurrency())\n";
    out.flush();
}


int main(int argc, char **argv) {
    std::optional<uint32_t> zeroes;
    std::optional<const char *> prefixfile;
    std::optional<const char *> outfile;
    uint32_t nthreads = 0;

    for (int opt; (opt = getopt(argc, argv, "hz:t:p:o:")) != -1; ) {
        switch (opt) {
        case 'h':
            usage(argv[0], std::cout);
            return 0;
        case 'z':
            zeroes = strtoul(optarg, nullptr, 0);
            if (zeroes.value() > 32) {
                std::cerr << argv[0] << ": invalid argument '"
                    << optarg << "' for '-z'" << std::endl;
                std::cerr << "Valid arguments are 0 to 32 (inclusive)."
                    << std::endl;
                return 1;
            }
            break;
        case 't':
            nthreads = strtoul(optarg, nullptr, 0);
            break;
        case 'p':
            prefixfile = optarg;
            break;
        case 'o':
            outfile = optarg;
            break;
        default:
            return 1;
        }
    }

    if (optind < argc) {
        std::cerr << argv[0] << ": extra operand '" << argv[optind]
            << "'" << std::endl;
        return 1;
    }

    if (!zeroes.has_value()) {
        std::cerr << argv[0] << ": missing required argument '-z'" << std::endl;
        return 1;
    }
    if (nthreads == 0) {
        nthreads = std::thread::hardware_concurrency();
        if (nthreads == 0) {
            std::cerr << argv[0] << ": unknown number of "
                << "hardware thread contexts." << std::endl;
            std::cerr << "Please specify '-t <threads>'." << std::endl;
            return 1;
        }
    }

    std::vector<uint32_t> prefix;
    if (prefixfile.has_value()) {
        std::ifstream fin(prefixfile.value(), std::ifstream::binary);
        if (!fin) {
            std::cerr << argv[0] << ": cannot open '" << prefixfile.value()
                    << "'" << std::endl;
            return 1;
        }
        std::stringstream sstr;
        sstr << fin.rdbuf();
        prefix = string_to_uint32(sstr.str());
    }

    md5::State state;
    for (size_t i = 0; i + 16 <= prefix.size(); i += 16)
        state = update(state, prefix.data() + i);

    std::cout << "Using " << nthreads << " threads." << std::endl;

    while (true) {
        if (prefix.size() % 16 + 4 <= 16) {
            uint32_t buf[16] = {};
            uint32_t psize = prefix.size() % 16;

            std::copy(prefix.end() - psize, prefix.end(), buf);
            for (uint32_t i = 1; psize + i + 3 <= 16; i++) {
                prepare_last_block(buf, buf + psize, buf + psize + i,
                        (prefix.size() + i) * 32);
                bool found = next_treasure_multithread(buf, buf + 16,
                        buf + psize, buf + psize + i, 1,
                        MD5_zeroes(state, zeroes.value()), nthreads);
                if (found) {
                    prefix.insert(prefix.end(), buf + psize, buf + psize + i);
                    goto lb_found;
                }
            }
        }
        prefix.resize(prefix.size() / 16 * 16 + 16);

        state = update(state, prefix.data() + prefix.size() - 16);
    }

lb_found:
    std::cout << "Treasure Found!" << std::endl;

    std::cout << "Treasure: ";
    print_treasure(std::cout, prefix.begin(), prefix.end());
    std::cout << std::endl;

    std::cout << "Hash: "
        << md5::md5(prefix.data(), prefix.size() * 32) << std::endl;

    if (outfile.has_value()) {
        std::cout << "Saving treasure to " << outfile.value() << std::endl;

        std::ofstream fout(outfile.value(), std::ofstream::binary);
        if (!fout) {
            std::cerr << argv[0] << ": cannot open '" << outfile.value()
                    << "'" << std::endl;
            return 1;
        }
        fout << uint32_to_string(prefix);
    }
}

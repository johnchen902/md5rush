#include <algorithm>
#include <atomic>
#include <boost/thread/sync_queue.hpp> // Put earlier to fix boost 1.65.1-2
#include <boost/thread/concurrent_queues/queue_adaptor.hpp>
#include <boost/thread/concurrent_queues/queue_views.hpp>
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

namespace {

template<typename Predicate>
struct Work {
    size_t max_count;
    unsigned mutable_begin, mutable_end;
    std::array<uint32_t, 16> array;
    Predicate pred;
    Work() = default;
};

struct Result {
    size_t count;
    std::optional<std::array<uint32_t, 16>> array;
    Result() = default;
    Result(size_t _count): count(_count), array() {}
    Result(size_t _count, std::array<uint32_t, 16> _array):
        count(_count), array(_array) {}
};

/**
 * Increase the sequence [first, last) by addend.
 *
 * Returns carry (when the sequence is full).
 */
template<typename Iterator>
auto add_sequence(Iterator first, Iterator last,
        typename std::iterator_traits<Iterator>::value_type addend) {
    for (; addend && first != last; ++first)
        addend = __builtin_add_overflow(*first, addend, std::addressof(*first));
    return addend;
}

template<typename Container>
bool next_work_array(Container &array, size_t begin, size_t end,
        typename Container::value_type n = 1) {
    return add_sequence(std::begin(array) + begin,
            std::begin(array) + end, n) == 0;
}

template<typename Predicate>
std::pair<Work<Predicate>, Work<Predicate>> split_work(
        Work<Predicate> work, size_t split_count) {
    Work work1 = work;
    work1.max_count = std::min(work1.max_count, split_count);
    work.max_count -= work1.max_count;
    if (!next_work_array(work.array, work.mutable_begin, work.mutable_end,
            split_count))
        work.max_count = 0;
    return std::make_pair(work1, work);
}

/**
 * Find 'next' treasure satisfying pred(work.array).
 */
template<typename Predicate>
Result next_treasure(Work<Predicate> work) {
    auto [max_count, mutable_begin, mutable_end, array, pred] = work;
    size_t count = 0;
    while (count < max_count) {
        count++;
        if (pred(array))
            return Result(count, array);
        if (!next_work_array(array, mutable_begin, mutable_end))
            break;
    }
    return Result(count);
}

template<typename Predicate>
void next_treasure_worker(
        boost::queue_front<Work<Predicate>> work_queue,
        boost::queue_back<Result> result_queue) {
    while (true) {
        Work<Predicate> work;
        auto status = work_queue.wait_pull(work);
        if (status != boost::queue_op_status::success)
            break;
        result_queue.push(next_treasure(work));
    }
}

template<typename Predicate>
Result next_treasure_master(Work<Predicate> work,
        boost::queue_back<Work<Predicate>> work_queue,
        boost::queue_front<Result> result_queue,
        size_t max_running_works, size_t block_size) {
    assert(max_running_works);

    bool exhausted = false;
    size_t running_works = 0;
    size_t count = 0;
    while (!exhausted || running_works) {
        if (!exhausted && running_works < max_running_works) {
            if (work.max_count == 0) {
                exhausted = true;
            } else {
                auto [work1, work2] = split_work(work, block_size);
                work_queue.push(work1);
                running_works++;
                work = work2;
            }
        } else {
            assert(running_works);

            Result result;
            result_queue.pull(result);
            running_works--;

            count += result.count;

            if (result.array.has_value())
                return Result(count, result.array.value());
        }
    }
    return Result(count);
}

void prepare_last_block(std::array<uint32_t, 16> &arr,
        uint32_t mutable_begin, uint32_t mutable_end, size_t nbits) {
    assert(mutable_begin <= mutable_end);
    assert(mutable_end + 3 <= 16);

    std::fill(arr.begin() + mutable_begin, arr.begin() + mutable_end, 0);
    arr[mutable_end] = 0x00000080;
    std::fill(arr.begin() + mutable_end + 1, arr.end() - 2, 0);
    arr[16 - 2] = nbits;
    arr[16 - 1] = nbits >> 32;
}

template<typename Pred_gen>
using Predicate_t = std::invoke_result_t<Pred_gen, md5::State>;
template<typename Pred_gen>
size_t next_treasure_main(std::vector<uint32_t> &prefix,
        boost::queue_back<Work<Predicate_t<Pred_gen>>> work_queue,
        boost::queue_front<Result> result_queue,
        size_t max_running_works, size_t block_size,
        Pred_gen pred_gen) {
    md5::State state;
    for (size_t i = 0; i + 16 <= prefix.size(); i += 16)
        state = update(state, prefix.data() + i);

    size_t count = 0;
    while (true) {
        if (prefix.size() % 16 + 4 <= 16) {
            std::array<uint32_t, 16> buf = {};
            uint32_t psize = prefix.size() % 16;

            std::copy(prefix.end() - psize, prefix.end(), buf.begin());
            for (uint32_t i = 1; psize + i + 3 <= 16; i++) {
                prepare_last_block(buf, psize, psize + i,
                         (prefix.size() + i) * 32);
                Work<Predicate_t<Pred_gen>> work {
                    std::numeric_limits<size_t>::max(),
                    psize, psize + i, buf, pred_gen(state) };

                Result result = next_treasure_master(work,
                        work_queue, result_queue,
                        max_running_works, block_size);
                count += result.count;
                if (result.array.has_value()) {
                    const auto &array = result.array.value();
                    prefix.insert(prefix.end(), array.begin() + psize,
                            array.begin() + psize + i);
                    return count;
                }
            }
        }
        prefix.resize(prefix.size() / 16 * 16 + 16);

        state = update(state, prefix.data() + prefix.size() - 16);
    }
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
    MD5_zeroes() = default;
    MD5_zeroes(md5::State _init_state, uint32_t _zeroes) :
        init_state(_init_state), zeroes(_zeroes) {}
    bool operator () (std::array<uint32_t, 16> array) const {
        md5::State state = md5::update(init_state, array.data());

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

bool read_prefix(std::vector<uint32_t> &prefix, const char *path) {
    std::ifstream fin(path, std::ifstream::binary);
    if (!fin)
        return false;
    std::stringstream sstr;
    sstr << fin.rdbuf();
    prefix = string_to_uint32(sstr.str());
    return true;
}

bool write_result(const std::vector<uint32_t> &result, const char *path) {
    std::ofstream fout(path, std::ofstream::binary);
    if (!fout)
        return false;
    fout << uint32_to_string(result);
    return true;
}

}

int main(int argc, char **argv) {
    std::optional<unsigned> zeroes;
    std::optional<const char *> prefixfile;
    std::optional<const char *> outfile;
    unsigned nthreads = 0;

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
        bool ret = read_prefix(prefix, prefixfile.value());
        if (!ret) {
            std::cerr << argv[0] << ": cannot read prefix from '"
                << prefixfile.value() << "'" << std::endl;
            return 1;
        }
    }

    boost::queue_adaptor<boost::sync_queue<Work<MD5_zeroes>>> work_queue;
    boost::queue_adaptor<boost::sync_queue<Result>> result_queue;
    std::vector<std::thread> threads;

    std::cout << "Using " << nthreads << " threads." << std::endl;
    for (unsigned i = 0; i < nthreads; i++)
        threads.emplace_back(next_treasure_worker<MD5_zeroes>,
            boost::queue_front<Work<MD5_zeroes>>(work_queue),
            boost::queue_back<Result>(result_queue));

    size_t count = next_treasure_main(prefix,
            boost::queue_back<Work<MD5_zeroes>>(work_queue),
            boost::queue_front<Result>(result_queue),
            2 * nthreads, 10000u,
            [zeroes](md5::State state) {
                return MD5_zeroes(state, zeroes.value());
            });

    work_queue.close();
    for (std::thread &t : threads)
        t.join();

    std::cout << "Treasure Found!" << std::endl;

    std::cout << "Treasure: ";
    print_treasure(std::cout, prefix.begin(), prefix.end());
    std::cout << std::endl;

    std::cout << "Hash: "
        << md5::md5(prefix.data(), prefix.size() * 32) << std::endl;

    std::cout << "Hash computed: " << count << std::endl;

    if (outfile.has_value()) {
        bool ret = write_result(prefix, outfile.value());
        if (!ret) {
            std::cerr << argv[0] << ": cannot write result to '"
                << outfile.value() << "'" << std::endl;
            return 1;
        }
        std::cout << "Treasure saved to " << outfile.value() << std::endl;
    }
}

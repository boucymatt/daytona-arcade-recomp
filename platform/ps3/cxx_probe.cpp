#include <bit>
#include <span>
#include <vector>
#include <stdexcept>
#include <cstdio>
thread_local int counter;
extern "C" int ps3_cxx_probe(FILE *log) {
    std::vector<unsigned> values{1, 2};
    std::span<unsigned> view(values);
    try { if (++counter) throw std::runtime_error("probe"); }
    catch (const std::exception &) {
        bool ok = view.size() == 2 && std::bit_cast<unsigned>(1.0f) == 0x3f800000 && counter == 1;
        if (log) { std::fprintf(log, "C++20 vector/span/bit_cast/exception/main-thread TLS %s\n", ok ? "PASS" : "FAIL"); std::fflush(log); }
        return ok ? 0 : 1;
    }
    return 1;
}
#ifndef PS3_CXX_PROBE
int main() { return ps3_cxx_probe(stdout); }
#endif

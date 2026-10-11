// Restricted libc++ hardware gate: not the full game ABI or exception test.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <numeric>
#include <span>
#include <string>
#include <vector>
#include "allocation_checks.h"

static volatile unsigned constructed;
struct Startup { Startup() { constructed = 0x1234; } } startup;
static unsigned destroyed;
struct Tracked { ~Tracked() { ++destroyed; } };

static bool record(FILE *log, const char *test, bool pass) {
    std::fprintf(log, "%s %s\n", test, pass ? "PASS" : "FAIL");
    std::fflush(log);
    return pass;
}

int main() {
    FILE *log = std::fopen("game:\\xbox360-cxx.log", "wb");
    if (!log) return 1;
    std::fputs("Restricted C++20 probe; exceptions/TLS/threads NOT tested\n", log);
    std::fflush(log);
    bool ok = record(log, "Static constructor", constructed == 0x1234);
    ok &= record(log, "Aligned allocation/overflow", allocation_checks());
    {
        std::vector<unsigned> values;
        for (unsigned i = 0; i < 4096; ++i) values.push_back(4095 - i);
        std::sort(values.begin(), values.end());
        std::span<const unsigned> view(values);
        ok &= record(log, "Vector growth/sort/span", view.size() == 4096 &&
                     view.front() == 0 && view.back() == 4095 &&
                     std::accumulate(view.begin(), view.end(), uint64_t{0}) == 8386560);
        std::string text(512, 'x');
        text.replace(100, 4, "Xbox");
        std::string copy = text;
        ok &= record(log, "Heap string copy", copy.size() == 512 && copy.substr(100, 4) == "Xbox");
        { auto item = std::make_unique<Tracked>(); }
        ok &= record(log, "RAII destruction", destroyed == 1);
        volatile double input = 4.0;
        ok &= record(log, "Double sqrt", std::sqrt(input) == 2.0);
    }
    std::fprintf(log, "C++ probe complete: %s\n", ok ? "PASS" : "FAIL");
    return std::fclose(log) == 0 && ok ? 0 : 1;
}

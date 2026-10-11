#include "allocation_checks.h"
#include <cstdio>

int main() {
    if (!allocation_checks()) return 1;
    auto *values = new unsigned[1024];
    for (unsigned i = 0; i < 1024; ++i) values[i] = i;
    for (unsigned i = 0; i < 1024; ++i) if (values[i] != i) return 2;
    delete[] values;
    std::puts("Allocation checks PASS (40 aligned cases, overflow, array)");
}

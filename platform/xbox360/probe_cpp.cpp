// Link gate, not a game. Failure is expected until the SDK has a C++ runtime.
#include <vector>
#include <span>
#include <stdexcept>
#include <cmath>

thread_local unsigned calls;
int main() {
    std::vector<int> values{1, 2, 3};
    std::span<int> view(values);
    ++calls;
    try {
        if (view.size() != 3 || std::sqrt(4.0) != 2.0) throw std::runtime_error("ABI probe");
    } catch (const std::exception &) {
        return 1;
    }
    return calls != 1;
}

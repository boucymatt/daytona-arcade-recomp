#pragma once
#include <cstdint>
struct FpReport { uint64_t hash; uint32_t checks, known_failures; };
FpReport run_fp_cases();

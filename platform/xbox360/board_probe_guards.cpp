// Test-only link guards, never a game backend. Fail if this memory-only
// probe reaches a scheduler, linked cabinet or generated TGP execution.
#include "runtime/m2_board.h"
namespace rt {
void Lockstep::add_callback(uint64_t, std::function<void()>) { throw Fatal("probe reached scheduler"); }
void CommBoard::cn_w(uint8_t) { throw Fatal("probe reached link control"); }
uint8_t CommBoard::fg_r() { throw Fatal("probe reached link receive"); }
void CommBoard::vblank() { throw Fatal("probe reached link vblank"); }
namespace tgpgen {
const uint32_t program_crc32 = 0;
const uint32_t program_words = 0;
void run(Tgp &, uint64_t) { throw Fatal("probe reached generated TGP execution"); }
}
}

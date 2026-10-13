import pathlib
import subprocess
import tempfile
import unittest


class CallbackStorage(unittest.TestCase):
    def test_long_run_reuses_callback_slots(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        source = r'''
#include "runtime/cpu.h"
#include "runtime/m2_replay_bus.h"
#include <algorithm>
#include <functional>
#include <vector>
#define private public
#include "runtime/lockstep.h"
#undef private
// No CPU execution is required to exercise scheduler callback lifetimes.
void rt::Cpu::check_immediate_irqs() {}
void rt::Cpu::execute_set_input(int, int) {}
int main() {
    rt::Cpu cpu(nullptr);
    rt::Lockstep scheduler(cpu);
    unsigned invoked=0;
    std::function<void()> next;
    next = [&] {
        ++invoked;
        if (invoked<1000000) scheduler.add_callback(scheduler.count+1,next);
    };
    scheduler.add_callback(1,next);
    for (unsigned i=1;i<=1000000;++i) {
        scheduler.count=i;
        scheduler.boundary();
        if(scheduler.calls_.size()>1 || scheduler.log_.size()>4097) return 1;
    }
    if(invoked!=1000000 || scheduler.calls_[0]) return 2;
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            src = pathlib.Path(folder) / 'test.cpp'
            src.write_text(source)
            exe = pathlib.Path(folder) / 'test'
            subprocess.run(['g++', '-std=c++20', '-O2', '-DPS3_GAME_PROBE',
                            '-DM2_XBOX360_HEADLESS', '-I'+str(root/'src'), str(src),
                            str(root/'src/runtime/lockstep.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

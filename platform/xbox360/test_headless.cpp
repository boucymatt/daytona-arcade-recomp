// The diagnostic restrictions must fail explicitly without changing free-run.
#include "runtime/lockstep.h"
#include "runtime/comm_board.h"
#include <cassert>
#include <cstdio>

struct Transport : rt::LinkTransport {
    void poll() override { assert(false); }
    bool rx_open() const override { return false; }
    bool tx_open() const override { return false; }
    int read(uint8_t *, int) override { return 0; }
    bool write(const uint8_t *, int) override { return false; }
};

int main() {
    rt::Cpu cpu(nullptr);
    bool rejected = false;
    try { rt::Lockstep trace(cpu, "not-a-trace"); }
    catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
    rt::Lockstep free(cpu);
    unsigned calls = 0;
    free.add_callback(5, [&] { ++calls; free.add_callback(6, [&] { ++calls; }); });
    free.count = 4; free.boundary(); assert(calls == 0);
    free.count = 5; free.boundary(); assert(calls == 1);
    free.count = 6; free.boundary(); assert(calls == 2);
    free.end_count = 7; free.count = 7;
    assert(free.boundary() && free.finished());
    uint8_t shared[0x4000] = {};
    rt::CommBoard comm(shared);
    comm.cn_w(1); comm.vblank(); (void)comm.fg_r();
    assert(comm.link() == rt::CommBoard::Link::Off);
    Transport transport;
    comm.set_transport(&transport);
    rejected = false;
    try { comm.vblank(); } catch (const std::runtime_error &) { rejected = true; }
    assert(rejected);
    puts("headless free-run and unsupported feature guards: PASS");
}

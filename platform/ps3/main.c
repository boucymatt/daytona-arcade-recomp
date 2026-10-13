/* ROM-free PS3 presentation/input bring-up; not the game runtime. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/process.h>
#include <rsx/rsx.h>
#include <sysutil/video.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#include "scale.h"
#if defined(PS3_RSX_PROBE) || defined(PS3_RSX_GAME)
#include "rsx_texture_probe.h"
#endif
#ifdef PS3_PERF_PROBE
#include <sys/systime.h>
static uint64_t perf_ticks(void) { uint64_t t; __asm__ volatile("mftb %0" : "=r"(t) :: "memory"); return t; }
#endif
static Ps3Scaler scaler;

SYS_PROCESS_PARAM(1001, 0x100000);
static int running = 1;
int ps3_probe_running(void) { return running; }
/* Gameplay sampling is separate from presentation; retain state when no new
 * pad packet arrives, but release inputs when the controller disconnects. */
int ps3_sample_controls(uint8_t *out) {
    static padData held;
    static unsigned gear = 1, previous_up, previous_down;
    if (!held.len) held.ANA_L_H = 128;
    padInfo info = {0};
    ioPadGetInfo(&info);
    if (!info.status[0]) { memset(&held, 0, sizeof held); held.ANA_L_H = 128; }
    else {
        padData next = {0};
        if (!ioPadGetData(0, &next) && next.len > 0) held = next;
    }
    if (held.BTN_SELECT && held.BTN_START) { running = 0; return 0; }
    int axis = (int)held.ANA_L_H - 128;
    if (axis > -12 && axis < 12) axis = 0;
    int steer = 128 + axis * 96 / 128;
    if (held.BTN_LEFT) steer = 32;
    if (held.BTN_RIGHT) steer = 224;
    if (held.BTN_R1 && !previous_up && gear < 4) ++gear;
    if (held.BTN_L1 && !previous_down && gear > 1) --gear;
    previous_up = held.BTN_R1; previous_down = held.BTN_L1;
    static const unsigned gears[5] = {0,2,1,6,5};
    out[0] = steer; out[1] = (held.BTN_CROSS || held.BTN_R2) ? 224 : 32;
    out[2] = (held.BTN_SQUARE || held.BTN_L2) ? 224 : 32;
    out[3] = 255; out[4] = 0x8f | (gears[gear] << 4); out[5] = 255;
    if (held.BTN_SELECT) out[3] &= ~1;
    if (held.BTN_START) out[3] &= ~0x10;
    if (held.BTN_TRIANGLE) out[3] &= ~0x20;
    if (held.BTN_CIRCLE) out[3] &= ~0x40;
    if (held.BTN_L3) out[3] &= ~4;
    if (held.BTN_R3) out[3] &= ~8;
    return running;
}
static FILE *logfile;
static gcmContextData *display_context;
static uint32_t *display_buffers[2];
static unsigned display_width, display_height, display_target;
static int display_pending;
#ifdef PS3_RSX_GAME
static int wait_flip(void);
static gcmSurface game_surface;
int ps3_present_geometry(const float *vertices, unsigned count, const Ps3GeometryBatch *batches, unsigned batch_count,
                         const void *tex0, const void *tex1, uint64_t generation,
                         const Ps3TilePacket *tiles) {
    static uint64_t wait_ticks, upload_ticks, vertex_ticks, submit_ticks;
    static unsigned frames, uploads, vertex_total, batch_total;
    static uint64_t layer_ticks;
    static unsigned layer_rows;
    static unsigned span_rebuilds;
    const uint64_t begin = perf_ticks();
    sysUtilCheckCallback();
    if (!running || count > 131072 || (display_pending && !wait_flip())) return 0;
    const uint64_t waited = perf_ticks();
    if ((count && !vertices) || (batch_count && !batches) || batch_count > count/3) return 0;
    for (unsigned i=0; i<batch_count; ++i)
        if (batches[i].first > count || batches[i].count > count-batches[i].first) return 0;
    /* The gameplay sampler owns pad packets in this mode. */
    if (!running) return 0;
    if (!tiles) return 0;
    if(tiles->material_count>65536 || (tiles->material_count && !tiles->material_colours)) return 0;
    if(tiles->material_count) memcpy(probe_shade_pixels,tiles->material_colours,tiles->material_count*16*sizeof(u32));
    const uint64_t layer_begin = perf_ticks();
    if (!upload_game_layers(tiles)) {
        fprintf(logfile,"FAIL: tile span capacity exceeded\n"); fflush(logfile); return 0;
    }
    layer_ticks += perf_ticks() - layer_begin;
    layer_rows += layer_rows_uploaded;
    span_rebuilds += layer_spans_rebuilt;
    const int upload_needed = count && (!probe_texture_valid || generation != probe_texture_generation);
    const uint64_t upload_begin = perf_ticks();
    if (count && !texture_game_upload(tex0, tex1, generation)) return 0;
    if(upload_needed) {
        fprintf(logfile,"RSX_TEXTURE changed_pairs=%u total_pairs=4096 uploaded_bytes=%u\n",
                probe_uploaded_bytes/8192,probe_uploaded_bytes);
        fflush(logfile);
    }
    const uint64_t uploaded = perf_ticks();
    if (count) memcpy(probe_game_vertices, vertices, count * 20 * sizeof(float));
    probe_game_count = count;
    probe_game_batches = batches; probe_game_batch_count = batch_count;
    __asm__ volatile("sync" ::: "memory");
    const uint64_t copied = perf_ticks();
    if (rsxAddressToOffset(display_buffers[display_target], &game_surface.colorOffset[0])) return 0;
    rsxSetSurface(display_context, &game_surface);
    rsxSetScissor(display_context, 0, 0, display_width, display_height);
    rsxSetDepthWriteEnable(display_context, GCM_TRUE);
    rsxSetClearDepthStencil(display_context, 0xffffffffu);
    rsxSetClearColor(display_context, 0xff101820);
    rsxClearSurface(display_context, GCM_CLEAR_Z | GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
    texture_probe_draw(display_context, display_width, display_height, 0);
    gcmResetFlipStatus();
    if (gcmSetFlip(display_context, display_target)) return 0;
    rsxFlushBuffer(display_context); gcmSetWaitFlip(display_context);
    display_pending = 1; display_target ^= 1;
    const uint64_t submitted = perf_ticks();
    wait_ticks += waited - begin;
    upload_ticks += uploaded - upload_begin;
    vertex_ticks += copied - uploaded;
    submit_ticks += submitted - copied;
    uploads += upload_needed;
    vertex_total += count; batch_total += batch_count;
    if (++frames % 60 == 0) {
        const double to_us = 1000000.0 / (double)sysGetTimebaseFrequency() / 60.0;
        fprintf(logfile, "RSX_TIMING frames=%u-%u wait_avg_us=%.0f texture_upload_avg_us=%.0f vertex_copy_avg_us=%.0f submit_avg_us=%.0f uploads=%u upload_bytes=%u vertices_avg=%u batches_avg=%u\n",
                frames-60, frames-1, wait_ticks*to_us, upload_ticks*to_us,
                vertex_ticks*to_us, submit_ticks*to_us, uploads, uploads*33554432u,
                vertex_total/60, batch_total/60);
        fflush(logfile);
        fprintf(logfile, "RSX_LAYERS frames=%u-%u upload_avg_us=%.0f changed_rows=%u total_rows=122880 span_rebuilds=%u\n",
                frames-60, frames-1, layer_ticks*to_us, layer_rows, span_rebuilds);
        fflush(logfile);
        layer_ticks = 0; layer_rows = 0;
        span_rebuilds=0;
        wait_ticks = upload_ticks = vertex_ticks = submit_ticks = 0;
        uploads = vertex_total = batch_total = 0;
    }
    return 1;
}
#endif
static void note(const char *text) {
    if (logfile) { fprintf(logfile, "%s\n", text); fflush(logfile); }
}
static void callback(u64 status, u64 param, void *data) {
    (void)param; (void)data;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}
static int wait_flip(void) {
    for (unsigned i = 0; i < 10000; ++i) {
        sysUtilCheckCallback();
        if (!gcmGetFlipStatus()) return 1;
        if (!running) return 0;
        usleep(200);
    }
    note("FAIL: flip timeout");
    return 0;
}
int ps3_present(const uint32_t *pixels, unsigned width, unsigned height) {
#ifdef PS3_PERF_PROBE
    static uint64_t wait_ticks, copy_ticks, submit_ticks;
    static unsigned count;
    uint64_t before = perf_ticks();
#endif
    sysUtilCheckCallback();
    if (!running || !pixels || !width || !height || (display_pending && !wait_flip())) return 0;
    for (unsigned port = 0; port < 7; ++port) {
        padData pad = {0};
        if (!ioPadGetData(port, &pad) && pad.len > 0 && pad.BTN_START && pad.BTN_SELECT)
            running = 0;
    }
    if (!running) return 0;
#ifdef PS3_PERF_PROBE
    uint64_t ready = perf_ticks();
#endif
    if (!ps3_scale(&scaler, display_buffers[display_target], display_width, display_height,
                   pixels, width, height)) { note("FAIL: scaler dimensions"); return 0; }
#ifdef PS3_PERF_PROBE
    uint64_t copied = perf_ticks();
#endif
    __asm__ volatile("sync" ::: "memory");
    gcmResetFlipStatus();
    if (gcmSetFlip(display_context, display_target)) { note("FAIL: game flip submit"); return 0; }
    rsxFlushBuffer(display_context);
    gcmSetWaitFlip(display_context);
    display_pending = 1;
    display_target ^= 1;
#ifdef PS3_PERF_PROBE
    uint64_t submitted = perf_ticks();
    wait_ticks += ready - before; copy_ticks += copied - ready; submit_ticks += submitted - copied;
    if (++count == 60) {
        const uint64_t frequency = sysGetTimebaseFrequency();
        if (logfile && frequency) {
            fprintf(logfile, "PRESENT frames=60 wait_input_avg_us=%u scale_copy_avg_us=%u submit_avg_us=%u\n",
                    (unsigned)(wait_ticks / count * 1000000 / frequency),
                    (unsigned)(copy_ticks / count * 1000000 / frequency),
                    (unsigned)(submit_ticks / count * 1000000 / frequency));
            fflush(logfile);
        }
        wait_ticks = copy_ticks = submit_ticks = 0; count = 0;
    }
#endif
    return 1;
}
int main(void) {
    logfile = fopen("/dev_hdd0/game/DAYTPS301/USRDIR/ps3-boot.log", "wb");
    if (!logfile) logfile = fopen("/dev_usb000/ps3-boot.log", "wb");
#ifdef PS3_RSX_GAME
    note("PS3 RSX revision 65; exact integer gamma conversion; native audio");
#elif defined(PS3_RSX_PROBE)
    note("PS3 RSX revision 34; textured depth-order test, no game");
#elif defined(PS3_PERF_PROBE)
    note("PS3 performance revision 31; lazy polygon shades, no audio");
#elif defined(PS3_GAME_DISPLAY)
    note("PS3 game display probe revision 30; lazy polygon shade parity, no audio");
#elif defined(PS3_GAME_PROBE)
    note("PS3 game runtime probe revision 1; no audio/presentation");
#else
    note("PS3 display/input probe revision 1; no ROM/audio/game logic");
#endif
#ifdef PS3_CXX_PROBE
    extern int ps3_cxx_probe(FILE *);
    note("Before C++20 runtime probe");
    if (ps3_cxx_probe(logfile)) { note("FAIL: C++ runtime"); return 1; }
#endif
    if (sysUtilRegisterCallback(0, callback, NULL)) return 1;
#if defined(PS3_GAME_PROBE) && !defined(PS3_GAME_DISPLAY)
    extern int ps3_game_probe(FILE *);
    note("Starting bounded game runtime test; returning to XMB afterwards");
    int result = ps3_game_probe(logfile);
    sysUtilUnregisterCallback(0);
    if (logfile) fclose(logfile);
    return result;
#endif
    void *io = memalign(1024 * 1024, 1024 * 1024);
    gcmContextData *context = NULL;
    if (!io || rsxInit(&context, 64 * 1024, 1024 * 1024, io)) {
        note("FAIL: rsxInit"); return 1;
    }
    note("RSX initialized");
    videoState state = {0};
    videoResolution resolution = {0};
    if (videoGetState(0, 0, &state) ||
        videoGetResolution(state.displayMode.resolution, &resolution) ||
        !resolution.width || !resolution.height || resolution.width > 1920 || resolution.height > 1080) {
        note("FAIL: video mode"); return 1;
    }
    videoConfiguration config = {0};
    config.resolution = state.displayMode.resolution;
#ifdef PS3_RSX_GAME
    // Do not inherit a costly 1080p XMB mode for the arcade renderer.
    // Preserve lower-resolution outputs and unsupported-display configurations.
    if (resolution.height > 720 &&
        videoGetResolutionAvailability(0, VIDEO_RESOLUTION_720, state.displayMode.aspect, 0) > 0) {
        videoResolution lower = {0};
        if (!videoGetResolution(VIDEO_RESOLUTION_720, &lower) && lower.width && lower.height) {
            resolution = lower;
            config.resolution = VIDEO_RESOLUTION_720;
        }
    }
    fprintf(logfile,"RSX output: %u x %u; original mode=%u selected mode=%u\n",
            resolution.width,resolution.height,state.displayMode.resolution,config.resolution);
    fflush(logfile);
#endif
    config.format = VIDEO_BUFFER_FORMAT_XRGB;
    config.aspect = state.displayMode.aspect;
    config.pitch = resolution.width * 4;
    if (videoConfigure(0, &config, NULL, 1)) { note("FAIL: videoConfigure"); return 1; }
    gcmSetFlipMode(GCM_FLIP_VSYNC);
    uint32_t *buffers[2];
    for (unsigned i = 0; i < 2; ++i) {
        u32 offset;
        buffers[i] = rsxMemalign(64, config.pitch * resolution.height);
        if (!buffers[i] || rsxAddressToOffset(buffers[i], &offset) ||
            gcmSetDisplayBuffer(i, offset, config.pitch, resolution.width, resolution.height)) {
            note("FAIL: display buffer"); return 1;
        }
    }
    if (ioPadInit(7)) { note("FAIL: pad init"); return 1; }
#ifdef PS3_RSX_PROBE
    /* No CPU writes to the color buffers: every visible pixel is cleared by RSX. */
    if (!texture_probe_init()) { note("FAIL: texture/shader setup"); return 1; }
    note("Texture and shaders loaded; near orange triangle must remain over checkerboard");
    gcmSurface surface = {0};
    surface.type = GCM_SURFACE_TYPE_LINEAR;
    surface.antiAlias = GCM_SURFACE_CENTER_1;
    surface.colorFormat = GCM_SURFACE_A8R8G8B8;
    surface.colorTarget = GCM_SURFACE_TARGET_0;
    surface.width = resolution.width; surface.height = resolution.height;
    for (unsigned i = 0; i < 4; ++i) {
        surface.colorLocation[i] = GCM_LOCATION_RSX;
        surface.colorPitch[i] = 64;
    }
    surface.colorPitch[0] = config.pitch;
    surface.depthFormat = GCM_SURFACE_ZETA_Z16;
    surface.depthLocation = GCM_LOCATION_RSX;
    surface.depthPitch = (resolution.width * 2 + 63) & ~63u;
    void *depth = rsxMemalign(64, surface.depthPitch * resolution.height);
    if (!depth || rsxAddressToOffset(depth, &surface.depthOffset)) {
        note("FAIL: RSX depth allocation"); return 1;
    }
    rsxSetColorMask(context, GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
    rsxSetDepthTestEnable(context, GCM_FALSE);
    const u32 bars[8] = {0xffffffff,0xffffff00,0xff00ffff,0xff00ff00,
                         0xffff00ff,0xffff0000,0xff0000ff,0xff000000};
    unsigned submitted = 0;
    int failed = 0;
    for (unsigned frame = 0; running && frame < 300; ++frame) {
        sysUtilCheckCallback();
        if (frame && !wait_flip()) { failed = 1; break; }
        for (unsigned port = 0; port < 7; ++port) {
            padData pad = {0};
            if (!ioPadGetData(port, &pad) && pad.len > 0 && pad.BTN_START && pad.BTN_SELECT) running = 0;
        }
        if (!running) break;
        if (rsxAddressToOffset(buffers[frame & 1], &surface.colorOffset[0])) { failed = 1; break; }
        rsxSetSurface(context, &surface);
        rsxSetScissor(context, 0, 0, resolution.width, resolution.height);
        rsxSetDepthWriteEnable(context, GCM_TRUE);
        rsxSetClearDepthStencil(context, 0xffffffffu);
        rsxClearSurface(context, GCM_CLEAR_Z);
        if (logfile && !(frame % 60)) {
            fprintf(logfile, "Depth test phase=%u order=%s\n", frame / 60,
                    (frame / 60) & 1 ? "near-first" : "far-first");
            fflush(logfile);
        }
        for (unsigned bar = 0; bar < 8; ++bar) {
            unsigned left = bar * resolution.width / 8, right = (bar + 1) * resolution.width / 8;
            rsxSetScissor(context, left, 0, right - left, resolution.height);
            rsxSetClearColor(context, bars[bar]);
            rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
        }
        rsxSetScissor(context, (frame * 8) % (resolution.width - 64), resolution.height / 2, 64, 64);
        rsxSetClearColor(context, 0xff808080);
        rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
        rsxSetScissor(context, 0, 0, resolution.width, resolution.height);
        texture_probe_draw(context, resolution.width, resolution.height, frame);
        gcmResetFlipStatus();
        if (gcmSetFlip(context, frame & 1)) { failed = 1; break; }
        rsxFlushBuffer(context);
        gcmSetWaitFlip(context);
        ++submitted;
        if (logfile && !(submitted % 60)) { fprintf(logfile, "RSX submitted=%u; visual confirmation required\n", submitted); fflush(logfile); }
    }
    if (submitted && !wait_flip()) failed = 1;
    note(failed ? "FAIL: RSX render/flip" : "RSX bounded run finished; returning to XMB");
    ioPadEnd(); sysUtilUnregisterCallback(0);
    if (logfile) fclose(logfile);
    return failed;
#endif
#ifdef PS3_GAME_DISPLAY
    display_context = context;
    display_width = resolution.width;
    display_height = resolution.height;
    display_buffers[0] = buffers[0]; display_buffers[1] = buffers[1];
#ifdef PS3_RSX_GAME
    if (!texture_probe_init()) { note("FAIL: game shader setup"); return 1; }
    game_surface.type = GCM_SURFACE_TYPE_LINEAR;
    game_surface.antiAlias = GCM_SURFACE_CENTER_1;
    game_surface.colorFormat = GCM_SURFACE_A8R8G8B8;
    game_surface.colorTarget = GCM_SURFACE_TARGET_0;
    game_surface.width = display_width; game_surface.height = display_height;
    for (unsigned i = 0; i < 4; ++i) {
        game_surface.colorLocation[i] = GCM_LOCATION_RSX;
        game_surface.colorPitch[i] = 64;
    }
    game_surface.colorPitch[0] = config.pitch;
    game_surface.depthFormat = GCM_SURFACE_ZETA_Z16;
    game_surface.depthLocation = GCM_LOCATION_RSX;
    game_surface.depthPitch = (display_width * 2 + 63) & ~63u;
    void *game_depth = rsxMemalign(64, game_surface.depthPitch * display_height);
    if (!game_depth || rsxAddressToOffset(game_depth, &game_surface.depthOffset)) {
        note("FAIL: game depth allocation"); return 1;
    }
    rsxSetColorMask(context, GCM_COLOR_MASK_R | GCM_COLOR_MASK_G | GCM_COLOR_MASK_B | GCM_COLOR_MASK_A);
#endif
    extern int ps3_game_probe(FILE *);
    int game_result = ps3_game_probe(logfile);
    if (display_pending) wait_flip();
    note(game_result ? "Run failed; returning to XMB" : "Run finished; holding final frame for up to 15 seconds");
    /* Always exit on failure. Also bound success hold if pad exit fails. */
    for (unsigned tick = 0; !game_result && running && tick < 900; ++tick) {
        sysUtilCheckCallback();
        for (unsigned port = 0; port < 7; ++port) {
            padData pad = {0};
            if (!ioPadGetData(port, &pad) && pad.len > 0 && pad.BTN_START && pad.BTN_SELECT) running = 0;
        }
        usleep(16000);
    }
    ioPadEnd(); sysUtilUnregisterCallback(0);
    if (logfile) fclose(logfile);
    return game_result;
#endif
    if (logfile) {
        fprintf(logfile, "Display %u x %u; two XRGB buffers\n", resolution.width, resolution.height);
        fflush(logfile);
    }
    const uint32_t colors[8] = {0xffffff,0xffff00,0x00ffff,0x00ff00,0xff00ff,0xff0000,0x0000ff,0};
    unsigned frame = 0, target = 0;
    int pending = 0, cross = 0;
    while (running) {
        sysUtilCheckCallback();
        if (!running || (pending && !wait_flip())) break;
        for (unsigned port = 0; port < 7; ++port) {
            padData pad = {0};
            if (!ioPadGetData(port, &pad) && pad.len > 0) {
                if (pad.BTN_START && pad.BTN_SELECT) running = 0;
                if (cross != (int)pad.BTN_CROSS) note(pad.BTN_CROSS ? "Cross pressed" : "Cross released");
                cross = pad.BTN_CROSS;
            }
        }
        if (!running) break;
        for (unsigned y = 0; y < resolution.height; ++y)
            for (unsigned x = 0; x < resolution.width; ++x) {
                uint32_t color = colors[x * 8 / resolution.width];
                if (y > resolution.height * 3 / 4)
                    color = ((x + frame * 4) % resolution.width < 32) ? 0xffffff : (cross ? 0x00ff00 : 0x202020);
                buffers[target][y * resolution.width + x] = color;
            }
        __asm__ volatile("sync" ::: "memory");
        gcmResetFlipStatus();
        if (gcmSetFlip(context, target)) { note("FAIL: flip submit"); break; }
        rsxFlushBuffer(context);
        gcmSetWaitFlip(context);
        pending = 1;
        target ^= 1;
        if (++frame == 120) note("120 flips submitted; visual confirmation required");
    }
    /* Buffers remain owned until process teardown; never free active scanout. */
    note("Leaving display probe");
    ioPadEnd();
    sysUtilUnregisterCallback(0);
    if (logfile) fclose(logfile);
    return 0;
}

// ROM-free readback of the production hardware renderer. In particular, a
// coloured solid alone cannot detect unreadable texture/tile storage buffers.
// Run with direct3d12 or vulkan as the first argument, or set SDL_GPU_DRIVER.
#include "app/gpu/gpu_renderer.h"
#include "app/gpu_device.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

void word(std::vector<uint8_t> &bytes, uint32_t index, uint16_t value) {
    bytes[index * 2] = uint8_t(value);
    bytes[index * 2 + 1] = uint8_t(value >> 8);
}

struct Scene {
    std::vector<uint8_t> tiles = std::vector<uint8_t>(0x10000);
    std::vector<uint8_t> chars = std::vector<uint8_t>(0x80000);
    std::vector<uint8_t> palette = std::vector<uint8_t>(0x4000);
    std::vector<uint8_t> xlat = std::vector<uint8_t>(0xc000);
    std::vector<uint8_t> luma = std::vector<uint8_t>(0x20000);
    std::vector<uint32_t> tex0 = std::vector<uint32_t>(0x80000, 0x55555555);
    std::vector<uint32_t> tex1 = std::vector<uint32_t>(0x80000, 0x55555555);
    std::vector<rt::GeoPoly> polys;
    std::unique_ptr<rt::Video> video;
    rt::VideoMem mem;

    Scene() {
        // The first character is transparent, the next two are solid pens 1
        // and 2. Layer 2 supplies a blue background; layer 0 has one green
        // foreground tile crossing the textured polygon.
        std::fill(chars.begin() + 32, chars.begin() + 64, uint8_t(0x11));
        std::fill(chars.begin() + 64, chars.begin() + 96, uint8_t(0x22));
        for (unsigned t = 0; t < 4096; ++t) word(tiles, 0x2000 + t, 1);
        word(tiles, 18 * 64 + 40, 0x8002);
        word(tiles, 0x5005, 0x8000); // disable unused layers 1 and 3
        word(tiles, 0x5007, 0x8000);
        word(palette, 1, 31u << 10); // tile blue
        word(palette, 2, 31u << 5);  // tile green
        word(palette, 0x1000, 31);   // polygon red
        // Nonzero addresses and luma-dependent colours make a missing storage
        // read distinguishable from a valid black texel.
        for (unsigned channel = 0; channel < 3; ++channel)
            for (unsigned component = 0; component < 32; ++component)
                for (unsigned light = 0; light < 256; ++light)
                    word(xlat, channel * 0x2000 + component * 256 + light,
                         uint16_t(64 + (191 * component * std::min(light, 63u)) / (31 * 63)));
        luma[40 * 4] = 127; // texel 5: (5 << 4) >> 1, clamped to full brightness
        luma[72 * 4] = 32;  // texel 9: a different, dimmer red
        mem = {palette.data(), xlat.data(), luma.data(), tex0.data(), tex1.data(), 1};
        polys.push_back(rectangle(100, 100, 380, 220, true));
        polys.push_back(rectangle(400, 240, 460, 300, false));
        video = std::make_unique<rt::Video>(tiles.data(), chars.data());
        video->xhout_w(uint16_t(-84));
        video->xvout_w(uint16_t(-130));
        video->colorxlat_w(0x40); // the board normally marks pens dirty on upload
        video->set_external_3d(true, true);
        update();
    }

    static rt::GeoPoly rectangle(int x0, int y0, int x1, int y1, bool textured) {
        rt::GeoPoly poly;
        poly.num_vertices = 4;
        poly.luma = 255;
        poly.texheader[0] = textured ? 0x40c0 : 0; // opaque, wrapping texture
        poly.viewport[2] = 495;
        poly.viewport[3] = 384;
        const float x[4] = {float(x0), float(x1), float(x1), float(x0)};
        const float y[4] = {float(y0), float(y0), float(y1), float(y1)};
        for (unsigned i = 0; i < 4; ++i)
            poly.v[i] = {x[i], 384.0f - y[i], {1.0f, 64.0f, 64.0f}};
        return poly;
    }

    void update() {
        video->frame_start();
        video->screen_update(polys, 0, mem);
    }
};

struct Resources {
    SDL_GPUDevice *dev = nullptr;
    SDL_GPUTexture *target = nullptr;
    SDL_GPUTransferBuffer *download = nullptr;
    app::GpuRenderer renderer;
    ~Resources() {
        renderer.shutdown();
        if (download) SDL_ReleaseGPUTransferBuffer(dev, download);
        if (target) SDL_ReleaseGPUTexture(dev, target);
        if (dev) SDL_DestroyGPUDevice(dev);
        SDL_Quit();
    }
};

bool patch(const uint32_t *pixels, int stride, int x, int y, uint32_t expected, const char *name) {
    for (int dy = 0; dy < 3; ++dy)
        for (int dx = 0; dx < 3; ++dx) {
            const uint32_t got = pixels[(y + dy) * stride + x + dx];
            if (got != expected) {
                std::fprintf(stderr, "%s at %d,%d: got %08x, expected %08x\n",
                             name, x + dx, y + dy, unsigned(got), unsigned(expected));
                return false;
            }
        }
    return true;
}

bool check_pixels(const uint32_t *pixels, unsigned frame) {
    // texture 9 -> luma 32 * 255 / 256 = 31 -> translation 157 -> gamma 124.
    const uint32_t texture = frame ? 0xff7c0000u : 0xffff0000u;
    bool ok = patch(pixels, rt::Video::W, 20, 20, 0xff0000ff, "back tile storage");
    ok = patch(pixels, rt::Video::W, 150, 150, texture, "texture/luma/translation storage") && ok;
    ok = patch(pixels, rt::Video::W, 322, 146, 0xff00ff00, "front tile storage") && ok;
    ok = patch(pixels, rt::Video::W, 420, 260, 0xffff0000, "solid polygon control") && ok;
    return ok;
}

bool render(Resources &gpu, const Scene &scene, unsigned frame) {
    constexpr int W = rt::Video::W, H = rt::Video::H;
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.dev);
    if (!cmd) return false;
    gpu.renderer.render(cmd, gpu.target, W, H, *scene.video);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion src{};
    src.texture = gpu.target;
    src.w = W;
    src.h = H;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = gpu.download;
    dst.pixels_per_row = W;
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) return false;
    const bool waited = SDL_WaitForGPUFences(gpu.dev, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu.dev, fence);
    if (!waited) return false;
    const auto *pixels = static_cast<const uint32_t *>(SDL_MapGPUTransferBuffer(gpu.dev, gpu.download, false));
    if (!pixels) return false;
    const bool ok = check_pixels(pixels, frame);
    SDL_UnmapGPUTransferBuffer(gpu.dev, gpu.download);
    return ok;
}

} // namespace

int main(int argc, char **argv) {
    Resources gpu;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SKIP: video unavailable: %s\n", SDL_GetError());
        return 77;
    }
    if (argc > 1) SDL_SetHint(SDL_HINT_GPU_DRIVER, argv[1]);
    gpu.dev = app::create_gpu_device();
    if (!gpu.dev) {
        std::fprintf(stderr, "SKIP: GPU unavailable: %s\n", SDL_GetError());
        return 77;
    }
    if (argc > 1 && std::strcmp(SDL_GetGPUDeviceDriver(gpu.dev), argv[1])) {
        std::fprintf(stderr, "FAIL: requested GPU driver %s, got %s\n", argv[1], SDL_GetGPUDeviceDriver(gpu.dev));
        return 1;
    }
    std::printf("GPU driver: %s\n", SDL_GetGPUDeviceDriver(gpu.dev));
#ifdef SDL_PROP_GPU_DEVICE_NAME_STRING
    std::printf("GPU device: %s\n", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu.dev),
                                                       SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown"));
#endif
    if (!gpu.renderer.init(gpu.dev, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)) {
        std::fprintf(stderr, "FAIL: %s\n", gpu.renderer.error().c_str());
        return 1;
    }
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    ti.width = rt::Video::W;
    ti.height = rt::Video::H;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    gpu.target = SDL_CreateGPUTexture(gpu.dev, &ti);
    SDL_GPUTransferBufferCreateInfo tbi{};
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tbi.size = rt::Video::W * rt::Video::H * 4;
    gpu.download = SDL_CreateGPUTransferBuffer(gpu.dev, &tbi);
    if (!gpu.target || !gpu.download) {
        std::fprintf(stderr, "FAIL: GPU allocation: %s\n", SDL_GetError());
        return 1;
    }
    Scene scene;
    auto reference = std::make_unique<rt::Video>(scene.tiles.data(), scene.chars.data());
    reference->xhout_w(uint16_t(-84));
    reference->xvout_w(uint16_t(-130));
    reference->colorxlat_w(0x40);
    reference->screen_update(scene.polys, 0, scene.mem);
    if (!check_pixels(reference->screen().data(), 0)) {
        std::fputs("FAIL: CPU reference fixture\n", stderr);
        return 1;
    }
    bool ok = render(gpu, scene, 0);
    std::fill(scene.tex0.begin(), scene.tex0.end(), 0x99999999u);
    std::fill(scene.tex1.begin(), scene.tex1.end(), 0x99999999u);
    ++scene.mem.tex_generation;
    scene.update();
    reference->frame_start();
    reference->screen_update(scene.polys, 0, scene.mem);
    if (!check_pixels(reference->screen().data(), 1)) {
        std::fputs("FAIL: changed CPU reference fixture\n", stderr);
        return 1;
    }
    ok = render(gpu, scene, 1) && ok;
    if (!ok) {
        std::fputs("FAIL: production renderer GPU readback\n", stderr);
        return 1;
    }
    std::puts("PASS: back/front tiles, textured/solid polygons and changed texture RAM");
    return 0;
}

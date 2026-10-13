#pragma once
#include "runtime/native_sound_engine.h"
#include <audio/audio.h>
#include <sys/thread.h>
#include <lv2/thread.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

// The worker alone owns sequencing/mixing; the game only queues UART bytes.
class Ps3NativeAudio {
    static constexpr uint32_t capacity = 16384;
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    std::array<uint8_t, capacity> bytes_{};
    std::atomic<uint32_t> read_{0}, write_{0}, stop_{0}, failed_{0};
    std::unique_ptr<snd::NativeSoundEngine> engine_;
    FILE *log_;
    sys_ppu_thread_t thread_{};
    sys_event_queue_t queue_{};
    sys_ipc_key_t key_{};
    audioPortConfig config_{};
    u32 port_{};
    bool initialized_=false, opened_=false, queued_=false, notified_=false, started_=false, threaded_=false;
    uint64_t blocks_=0, overflow_=0;
    uint64_t nonzero_=0, bad_samples_=0;
    float peak_=0;
    static std::vector<uint8_t> load(const std::string &dir, const char *name, size_t size) {
        const auto path = dir + "/" + name;
        std::unique_ptr<FILE, decltype(&fclose)> f(fopen(path.c_str(), "rb"), fclose);
        if (!f) throw std::runtime_error("Missing native audio asset: " + path);
        std::vector<uint8_t> data(size);
        if (fread(data.data(), 1, size, f.get()) != size || fgetc(f.get()) != EOF || ferror(f.get()))
            throw std::runtime_error("Invalid native audio asset: " + path);
        return data;
    }
    static void check(s32 result, const char *operation) {
        if (result) throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
    }
    static void entry(void *arg) {
        auto &self = *static_cast<Ps3NativeAudio *>(arg);
        try { self.work(); } catch (...) { self.failed_.store(1); }
        sysThreadExit(0);
    }
    void work() {
        std::array<uint8_t, capacity> pending;
        float samples[512];
        while (!stop_.load(std::memory_order_acquire)) {
            // readIndex is a shared address, not a block number. Follow the
            // audio server, including after delayed or missed notifications.
            const u64 current = *reinterpret_cast<volatile u64 *>(uintptr_t(config_.readIndex));
            const unsigned block = unsigned((current + 1) % config_.numBlocks);
            sys_event_t event;
            const auto rc = sysEventQueueReceive(queue_, &event, 100000);
            if (stop_.load(std::memory_order_acquire)) break;
            if (rc) { failed_.store(uint32_t(rc)); break; }
            const auto r = read_.load(std::memory_order_relaxed);
            const auto w = write_.load(std::memory_order_acquire);
            const auto count = w-r;
            for (uint32_t i=0; i<count; ++i) pending[i]=bytes_[(r+i)%capacity];
            read_.store(w, std::memory_order_release);
            if (count) engine_->send(pending.data(), count);
            engine_->render(samples, 256);
            for (float &sample : samples) {
                sample *= 0.8f;
                if (!std::isfinite(sample)) { sample=0; ++bad_samples_; }
                const float magnitude=std::fabs(sample);
                if (magnitude>peak_) peak_=magnitude;
                nonzero_ += magnitude>0.00001f;
            }
            auto *destination = reinterpret_cast<float *>(uintptr_t(config_.audioDataStart));
            memcpy(destination + block*512, samples, sizeof(samples));
            __sync_synchronize();
            ++blocks_;
        }
    }
    void close() noexcept {
        stop_.store(1, std::memory_order_release);
        if (threaded_) { u64 result; sysThreadJoin(thread_, &result); threaded_=false; }
        if (started_) audioPortStop(port_);
        if (notified_) audioRemoveNotifyEventQueue(key_);
        if (opened_) audioPortClose(port_);
        if (queued_) sysEventQueueDestroy(queue_, 0);
        if (initialized_) audioQuit();
        started_=notified_=opened_=queued_=initialized_=false;
    }
public:
    Ps3NativeAudio(const std::string &dir, FILE *log) : log_(log) {
        engine_ = std::make_unique<snd::NativeSoundEngine>(load(dir,"sound_program.bin",262144),
            load(dir,"pcm1.bin",4194304), load(dir,"pcm2.bin",4194304));
        try {
            check(audioInit(), "audioInit"); initialized_=true;
            audioPortParam params{};
            params.numChannels=2; params.numBlocks=8; params.attrib=AUDIO_PORT_INITLEVEL; params.level=1.f;
            check(audioPortOpen(&params,&port_), "audioPortOpen"); opened_=true;
            check(audioGetPortConfig(port_,&config_), "audioGetPortConfig");
            if (!config_.readIndex || !config_.audioDataStart || config_.numBlocks != 8 || config_.channelCount != 2)
                throw std::runtime_error("Unexpected PS3 audio port configuration");
            memset(reinterpret_cast<void *>(uintptr_t(config_.audioDataStart)),0,config_.portSize);
            check(audioCreateNotifyEventQueue(&queue_,&key_), "audioCreateNotifyEventQueue"); queued_=true;
            check(audioSetNotifyEventQueue(key_), "audioSetNotifyEventQueue"); notified_=true;
            sysEventQueueDrain(queue_);
            check(audioPortStart(port_), "audioPortStart"); started_=true;
            char name[]="daytona-native-audio";
            check(sysThreadCreate(&thread_,entry,this,1000,131072,THREAD_JOINABLE,name), "audio thread"); threaded_=true;
            fprintf(log_,"AUDIO native sequencer/mixer: 48000 Hz stereo, independent event-driven thread\n"); fflush(log_);
        } catch (...) { close(); throw; }
    }
    void send(const std::vector<uint8_t> &data) {
        if (failed_.load()) throw std::runtime_error("Native audio worker failed: " + std::to_string(failed_.load()));
        const auto w=write_.load(std::memory_order_relaxed), r=read_.load(std::memory_order_acquire);
        if (data.size()>capacity-(w-r)) { ++overflow_; throw std::runtime_error("Native audio command queue overflow"); }
        for (size_t i=0;i<data.size();++i) bytes_[(w+i)%capacity]=data[i];
        write_.store(w+uint32_t(data.size()),std::memory_order_release);
    }
    ~Ps3NativeAudio() {
        close();
        const auto stats=engine_->stats();
        fprintf(log_,"AUDIO stopped blocks=%llu bytes=%llu notes=%llu invalid=%llu unsupported=%llu overflow=%llu worker_error=%u\n",
            (unsigned long long)blocks_,(unsigned long long)stats.bytes,(unsigned long long)stats.notes,
            (unsigned long long)stats.invalid,(unsigned long long)stats.unsupported,(unsigned long long)overflow_,failed_.load());
        fflush(log_);
        fprintf(log_,"AUDIO signal peak=%f nonzero_samples=%llu invalid_samples=%llu\n",
            double(peak_),(unsigned long long)nonzero_,(unsigned long long)bad_samples_);
        fflush(log_);
    }
};

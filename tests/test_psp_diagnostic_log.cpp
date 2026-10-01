#include "../platform/psp/diagnostic_log.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <thread>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
namespace {
struct Io {
    std::map<std::string, std::string> files;
    std::vector<int> writes;
    std::vector<std::string> calls;
    std::string opened;
    size_t write_index = 0;
    int open_error = 0, seek_error = 0, close_error = 0, sync_error = 0;
    bool active = false;
} io;
using Log = psp::DiagnosticLog;
void reset() { io = {}; }
void init(Log& log, const char* cwd = "ms0:/PSP/GAME/DAYTONA") {
    CHECK(log.initialize(cwd)); CHECK(log.ready());
}
}
SceUID sceIoOpen(const char* path, int flags, SceMode mode) {
    CHECK(!io.active && mode == 0777);
    CHECK(flags == (PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND));
    CHECK(!(flags & PSP_O_TRUNC));
    CHECK(std::string(path).find(":/") != std::string::npos);
    io.calls.push_back("open");
    if (io.open_error) return io.open_error;
    io.active = true; io.opened = path; io.files.try_emplace(path);
    return 0; // Zero is a valid native descriptor.
}
int sceIoLseek32(SceUID file, int offset, int whence) {
    CHECK(file == 0 && io.active && offset == 0 && whence == PSP_SEEK_END);
    io.calls.push_back("seek");
    return io.seek_error ? io.seek_error : int(io.files.at(io.opened).size());
}
int sceIoWrite(SceUID file, const void* bytes, SceSize length) {
    CHECK(file == 0 && io.active && length);
    io.calls.push_back("write");
    const int result = io.write_index < io.writes.size() ? io.writes[io.write_index++] : int(length);
    if (result > 0 && unsigned(result) <= length)
        io.files.at(io.opened).append(static_cast<const char*>(bytes), size_t(result));
    return result;
}
int sceIoClose(SceUID file) {
    CHECK(file == 0 && io.active); io.active = false;
    io.calls.push_back("close"); return io.close_error;
}
int sceIoSync(const char* device, unsigned int flags) {
    CHECK(!io.active && flags == 0);
    CHECK(device == std::string("ms0:") || device == std::string("ef0:"));
    io.calls.push_back("sync"); return io.sync_error;
}

int main() {
    {
        reset(); Log log; init(log);
        CHECK(std::string(log.path(false)) == "ms0:/PSP/GAME/DAYTONA/psp-diagnostic.log");
        CHECK(std::string(log.path(true)) == "ms0:/PSP/GAME/DAYTONA/psp-stall.log");
        // Worker uses absolute paths without invoking any cwd function. Main
        // reads only atomics while it runs; the fake filesystem is writer-only.
        std::atomic<bool> done{false};
        std::thread worker([&] { CHECK(log.write(true, "stall\n", 6)); done = true; });
        while (!done.load()) { CHECK(log.error() == 0); CHECK(log.records() <= 1); }
        worker.join();
        CHECK(io.files.at(log.path(true)) == "stall\n");
        CHECK(log.records() == 1 && log.error() == 0);
        CHECK((io.calls == std::vector<std::string>{"open", "seek", "write", "close", "sync"}));
        CHECK(log.write(false, "start\n", 6, false));
        CHECK(log.records() == 2 && io.calls.back() == "close");
    }
    for (const char* cwd : {"ms0:/", "ms0:/PSP/GAME/DAYTONA/", "ef0:/PSP/GAME/My Game///"}) {
        reset(); Log log; init(log, cwd);
        CHECK(log.write(false, "a", 1));
        CHECK(std::string(log.path(false)).find("//") == std::string::npos);
    }
    for (const char* cwd : {"", "relative", "/PSP/GAME", "ms0:", "ms0:relative", ":/path",
                            "ms0//:/path", "ms0:/a/../b", "ms0:/./a", "ms0:/a\\b", "ms0:/a:b", "ms0:/a\nb"}) {
        reset(); Log log; CHECK(!log.initialize(cwd));
        CHECK(!log.ready() && log.error() == Log::kInvalidPath && io.calls.empty());
    }
    {
        reset(); Log log; CHECK(!log.initialize(nullptr)); CHECK(log.error() == Log::kInvalidPath);
        CHECK(!log.write(false, "a", 1)); CHECK(log.error() == Log::kInvalidPath);
        CHECK(io.calls.empty());
    }
    {
        reset(); Log log; CHECK(!log.write(false, "a", 1));
        CHECK(log.error() == Log::kNotInitialized && io.calls.empty());
    }
    {
        reset(); Log log;
        char unterminated[Log::kPathCapacity];
        std::memset(unterminated, 'a', sizeof(unterminated));
        CHECK(!log.initialize(unterminated)); CHECK(log.error() == Log::kPathTooLong);
    }
    {
        reset(); Log log; const std::string cwd = "ms0:/" + std::string(Log::kPathCapacity - 6, 'a');
        CHECK(!log.initialize(cwd.c_str())); CHECK(log.error() == Log::kPathTooLong);
    }
    {
        reset(); Log log; const std::string cwd = std::string(Log::kDeviceCapacity, 'a') + ":/";
        CHECK(!log.initialize(cwd.c_str())); CHECK(log.error() == Log::kPathTooLong);
    }
    {
        reset(); Log log;
        const std::string cwd = "ms0:/" + std::string(Log::kPathCapacity - 5 - 1 - sizeof("psp-diagnostic.log"), 'a');
        init(log, cwd.c_str()); CHECK(std::strlen(log.path(false)) == Log::kPathCapacity - 1);
        CHECK(!log.initialize("ef0:/")); CHECK(log.error() == Log::kInvalidPath);
        CHECK(log.path(false)[0] == 'm');
    }
    {
        reset(); Log log; init(log); io.writes = {1, 2, 1};
        CHECK(log.write(false, "hello", 5)); CHECK(io.files.at(log.path(false)) == "hello");
        CHECK(log.write(false, "!", 1)); CHECK(io.files.at(log.path(false)) == "hello!");
        CHECK(log.records() == 2);
    }
    for (int result : {0, -88, 99}) {
        reset(); Log log; init(log); io.writes = {2, result}; io.close_error = -90; io.sync_error = -91;
        CHECK(!log.write(false, "hello", 5));
        CHECK(log.error() == (result == 0 ? Log::kNoWriteProgress : result < 0 ? -88 : Log::kInvalidWriteCount));
        CHECK(io.files.at(log.path(false)) == "he");
        CHECK(log.records() == 0 && !io.active);
        CHECK(io.calls.back() == "sync");
    }
    for (unsigned which = 0; which < 4; ++which) {
        reset(); Log log; init(log);
        if (which == 0) io.open_error = -101;
        if (which == 1) io.seek_error = -102;
        if (which == 2) io.close_error = -103;
        if (which == 3) io.sync_error = -104;
        CHECK(!log.write(false, "abc", 3)); CHECK(log.error() == -101 - int(which));
        CHECK(log.records() == 0 && !io.active);
        if (which == 0) CHECK(io.calls.size() == 1);
        if (which == 1) CHECK((io.calls == std::vector<std::string>{"open", "seek", "close"}));
        if (which > 1) CHECK(io.files.at(log.path(false)) == "abc");
    }
    {
        reset(); Log log; init(log); io.files[log.path(false)] = std::string(Log::kFileLimit - 3, 'x');
        CHECK(log.write(false, "end", 3)); CHECK(io.files.at(log.path(false)).size() == Log::kFileLimit);
        const auto before = io.files.at(log.path(false));
        CHECK(!log.write(false, "!", 1)); CHECK(log.error() == Log::kFileFull);
        CHECK(io.files.at(log.path(false)) == before && log.records() == 1);
        CHECK(log.write(true, "separate", 8)); CHECK(log.records() == 2);
        CHECK(log.error() == Log::kFileFull); // First failure is sticky.
    }
    for (size_t size : {Log::kFileLimit - 2, Log::kFileLimit + 1}) {
        reset(); Log log; init(log); io.files[log.path(false)] = std::string(size, 'x');
        CHECK(!log.write(false, "abcd", 4)); CHECK(log.error() == Log::kFileFull);
        CHECK(io.files.at(log.path(false)) == std::string(size, 'x'));
        CHECK((io.calls == std::vector<std::string>{"open", "seek", "close"}));
    }
    {
        reset(); Log log; init(log); CHECK(!log.write(false, nullptr, 1));
        CHECK(log.error() == Log::kInvalidRecord && io.calls.empty());
    }
    {
        reset(); Log log; init(log); CHECK(!log.write(false, "", 0));
        CHECK(log.error() == Log::kInvalidRecord && io.calls.empty());
    }
    {
        reset(); Log log; init(log); CHECK(!log.write(false, "a", Log::kFileLimit + 1));
        CHECK(log.error() == Log::kFileFull && io.calls.empty());
    }
    std::puts("PSP diagnostic log tests passed");
}

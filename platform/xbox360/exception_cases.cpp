// Same exception semantics test on host and (once the ABI links) Xbox.
// No ROM, SDL or standard-library containers involved.
static unsigned destroyed;
struct Scope {
    unsigned digit;
    ~Scope() { destroyed = destroyed * 10 + digit; }
};
struct Error { int value; };

__attribute__((noinline)) static void raise_error(int value) {
    Scope inner{2};
    throw Error{value};
}
__attribute__((noinline)) static void nested(int value) {
    Scope outer{1};
    raise_error(value);
}
int run_exception_cases() {
    destroyed = 0;
    try { nested(73); return 1; }
    catch (int) { return 2; }
    catch (const Error &error) { if (error.value != 73 || destroyed != 21) return 3; }
    destroyed = 0;
    try {
        try { nested(91); return 4; }
        catch (const Error &) { throw; }
    } catch (const Error &error) {
        if (error.value != 91 || destroyed != 21) return 5;
    }
    bool caught = false;
    try { throw 17; } catch (...) { caught = true; }
    return caught ? 0 : 6;
}
#ifdef XBOX_EXCEPTION_HOST_TEST
int main() {
    for (unsigned i = 0; i < 1000; ++i)
        if (int result = run_exception_cases()) return result;
    return 0;
}
#endif

#include <stdio.h>
int run_exception_cases();

int main() {
    // Close the start marker before any throw, so an unwind failure leaves
    // a persistent diagnostic even if process termination bypasses stdio.
    FILE *log = fopen("game:\\xbox360-exception.log", "wb");
    if (!log) return 1;
    if (fputs("Exception probe START: single thread, not game-ready\n", log) < 0) {
        fclose(log);
        return 1;
    }
    if (fclose(log) != 0) return 1;
    int result = run_exception_cases();
    log = fopen("game:\\xbox360-exception.log", "ab");
    if (!log) return 1;
    fprintf(log, "Typed catch, nested cleanup, rethrow, catch-all: %s (code=%d)\n",
            result ? "FAIL" : "PASS", result);
    fprintf(log, "Exception probe complete: %s\n", result ? "FAIL" : "PASS");
    return fclose(log) == 0 && result == 0 ? 0 : 1;
}

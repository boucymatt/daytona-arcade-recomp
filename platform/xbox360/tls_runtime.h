#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Initialize before starting workers; cleanup on each participating thread
// before ExTerminateThread. Shutdown only after every worker has joined.
int xbox_tls_init(void);
void xbox_tls_thread_cleanup(void);
int xbox_tls_shutdown(void);
#ifdef __cplusplus
}
#endif

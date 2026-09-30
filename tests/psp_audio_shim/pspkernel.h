#pragma once
#include <cstdint>
using SceUID = int;
using SceSize = unsigned;
#define PSP_THREAD_ATTR_USER 0x80000000u
#define PSP_THREAD_ATTR_VFPU 0x4000u
int sceKernelCreateThread(const char*, int (*)(SceSize, void*), int, int, unsigned, void*);
int sceKernelStartThread(int, SceSize, void*);
int sceKernelWaitThreadEnd(int, unsigned*);
int sceKernelDeleteThread(int);
int sceKernelDelayThread(unsigned);
void sceKernelDcacheWritebackRange(void*, unsigned);
unsigned sceKernelGetSystemTimeLow();

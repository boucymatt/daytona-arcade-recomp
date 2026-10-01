#pragma once

using SceUID = int;
using SceSize = unsigned int;
using SceMode = unsigned int;
inline constexpr int PSP_O_WRONLY = 0x0002, PSP_O_APPEND = 0x0100;
inline constexpr int PSP_O_CREAT = 0x0200, PSP_O_TRUNC = 0x0400;
inline constexpr int PSP_SEEK_END = 2;

SceUID sceIoOpen(const char*, int, SceMode);
int sceIoLseek32(SceUID, int, int);
int sceIoWrite(SceUID, const void*, SceSize);
int sceIoClose(SceUID);
int sceIoSync(const char*, unsigned int);

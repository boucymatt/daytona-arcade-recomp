#pragma once
#define PSP_AUDIO_VOLUME_MAX 0x8000
#define SCE_AUDIO_ERROR_OUTPUT_BUSY 0x80260002u
int sceAudioSRCChReserve(int frames, int rate, int channels);
int sceAudioSRCChRelease();
int sceAudioSRCOutputBlocking(int volume, void* buffer);

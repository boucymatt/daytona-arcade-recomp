# PS3 native audio bring-up (revision 56)

Revision 55 received commands and rendered blocks without worker errors, but
hardware testing reported silence. Revision 56 follows the shared audio server
read cursor when selecting the next block, matching PSL1GHT's audiosync sample,
instead of assuming notification count and playback position remain aligned.
Shutdown logging now includes peak amplitude, nonzero samples and invalid
samples to distinguish silent synthesis from an output-path problem.

The RSX game target uses the shared NativeSoundEngine, NativeSoundSequencer and
NativeSampleMixer. It does not execute the sound CPU or emulate the sound chips.
The PSL1GHT audio notification queue drives an independent joinable PPU thread
at 48 kHz, stereo float, 256 frames per block. Game UART commands cross a bounded
16 KiB SPSC queue. Mixing is not performed by the render thread.

Place the privately imported daytona93 sound_program.bin (262144 bytes),
pcm1.bin and pcm2.bin (4194304 bytes each) in the installed USRDIR/roms folder.
These files are not included in the package or source repository.

Select + Start or XMB exit joins the worker before releasing the engine and
audio port. The log records initialization, mixed blocks, command bytes, notes,
invalid/unsupported commands, queue overflows and worker errors. Initialization
failures are reported instead of silently substituting emulated audio.

Hardware validation is still required: confirm music and effects, continuous
audio during slow rendering, and clean exit/relaunch. Native synthesis remains
an approximation of the original sound hardware. This change does not establish
60 FPS gameplay or correct the existing approximate 3D lighting.

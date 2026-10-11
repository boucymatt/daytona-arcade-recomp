/* Link first. Bare-metal libunwind scans these statically linked frames. */
.section .eh_frame,"dr"
.p2align 2
.globl __eh_frame_start
__eh_frame_start:

/* No lookup index: identical bounds tell libunwind to scan frame records. */
.section .rdata,"dr"
.p2align 2
.globl __eh_frame_hdr_start
.globl __eh_frame_hdr_end
__eh_frame_hdr_start:
__eh_frame_hdr_end:
.long 0

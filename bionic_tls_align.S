// bionic_tls_align.S
//
// Workaround for Android Bionic TLS alignment requirement.
// Bionic's __libc_init checks PT_TLS.p_align and aborts if it's less than
// 64 (ARM64) or 32 (ARM32). NDK's static libc.a produces TLS segments with
// alignment 8, causing "executable's TLS segment is underaligned" on QEMU
// user-mode and Docker multiarch environments.
//
// This file forces .tdata section alignment via assembly .p2align directive,
// which propagates to PT_TLS segment alignment in the final ELF.
// Clang's __attribute__((aligned(N))) on __thread variables does NOT affect
// PT_TLS alignment — only section-level .p2align works.
//
// Include this file in the NDK static link step:
//   aarch64-linux-android26-clang++ ... -static -o test test.cpp bionic_tls_align.S

#if defined(__aarch64__)
    .section .tdata, "awT", @progbits
    .p2align 6          // 2^6 = 64 bytes — ARM64 Bionic minimum
    .type __bionic_tls_pad, @object
    .size __bionic_tls_pad, 64
__bionic_tls_pad:
    .zero 64

#elif defined(__arm__)
    .section .tdata, "awT", %progbits
    .p2align 5          // 2^5 = 32 bytes — ARM32 Bionic minimum
    .type __bionic_tls_pad, %object
    .size __bionic_tls_pad, 32
__bionic_tls_pad:
    .zero 32
#endif

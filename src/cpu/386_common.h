/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Common 386 CPU code.
 *
 *
 *
 * Authors: Sarah Walker, <https://pcem-emulator.co.uk/>
 *          Miran Grca, <mgrca8@gmail.com>
 *
 *          Copyright 2008-2019 Sarah Walker.
 *          Copyright 2016-2019 Miran Grca.
 */
#ifndef _386_COMMON_H_
#define _386_COMMON_H_

#include <stddef.h>
#include <inttypes.h>
#include <string.h>

/* x86 instruction bytes/immediates/displacements have no alignment
 * guarantee - pccache2[a] can land on any byte address. A raw
 * *(uint16_t*)/(uint32_t*)ptr dereference is an unaligned load, which
 * x86-64/ARM64 tolerate silently but which traps ("Load address
 * misaligned") on this 32-bit RISC-V target. A memcpy() of a constant
 * size is NOT a reliable fix here - GCC on this target still recognized
 * the 2/4-byte memcpy as "just a load" and emitted the same unaligned
 * instruction (confirmed on real hardware). Reconstruct the little-endian
 * value from single-byte loads instead - each byte access is trivially
 * aligned, so there is no load for the compiler to fuse back together. */
/* User-requested experiment (2026-07-30): -Og (this target's build, see
 * the -O2/-O3-are-slower-here note in project_esp32_port_plan memory)
 * treats "inline" as a hint it's free to ignore, and on this extremely
 * hot path (every unaligned memory load in the interpreter) a real CALL
 * instruction's overhead is no longer negligible. #define forces true
 * textual substitution at every call site - the preprocessor doesn't
 * "decide" whether to inline, there's no function left to call. GCC
 * statement-expressions (({ ... })) evaluate p exactly once into a local,
 * matching the original functions' semantics exactly - a naive
 * object-like macro would evaluate p up to 4 times (once per byte),
 * which is only safe here because callers happen to pass side-effect-free
 * pointer expressions, but there's no reason to rely on that staying true
 * forever. */
#define mem_load_u16_unaligned(p)                                                 \
    ({                                                                            \
        const uint8_t *mlu16_b_ = (const uint8_t *) (p);                         \
        (uint16_t) ((uint16_t) mlu16_b_[0] | ((uint16_t) mlu16_b_[1] << 8));      \
    })

#define mem_load_u32_unaligned(p)                                                          \
    ({                                                                                     \
        const uint8_t *mlu32_b_ = (const uint8_t *) (p);                                  \
        (uint32_t) ((uint32_t) mlu32_b_[0] | ((uint32_t) mlu32_b_[1] << 8)                 \
                    | ((uint32_t) mlu32_b_[2] << 16) | ((uint32_t) mlu32_b_[3] << 24));    \
    })

/* pccache/pccache2 cache a single page's worth of exec-mapping lookup so
 * repeated fetches within the same page skip getpccache()/MEM_EXEC_LOOKUP()
 * entirely. That is only safe if nothing remaps this page's exec pointer
 * behind the cache's back - some chipset shadow-RAM/bank-switch paths
 * call flushmmucache_nopc() (invalidates readlookup2/writelookup2 but
 * deliberately not pccache/pccache2) after changing a mapping, which can
 * leave pccache2 pointing at a stale buffer for the SAME page number.
 * Re-check the page's real exec pointer on every access (one array read
 * + one pointer compare) instead of trusting the page-number match alone,
 * and force a re-resolve on mismatch instead of using a dangling pointer. */
/* NOTE: re-verifying MEM_EXEC_LOOKUP() on every single byte/word/dword
 * access (not just on a pccache page-miss) was tried and measured on real
 * hardware to be far more expensive than the resolve it's meant to avoid -
 * pccache_valid/invalid counters showed the cache is valid ~100% of the
 * time, yet cpu_exec() got *slower* than the no-pccache2 fallback path.
 * The real fix belongs at the source: whichever chipset code changes an
 * exec mapping must invalidate pccache/pccache2 right then (see opti495.c),
 * not have every reader pay to re-check it. Kept as a plain page-number
 * check, matching upstream. */
#define PCCACHE_VALID(a) (((a) >> 12) == pccache)

#ifdef OPS_286_386
#    define readmemb_n(s, a, b)     readmembl_no_mmut_2386((s) + (a), b)
#    define readmemw_n(s, a, b)     readmemwl_no_mmut_2386((s) + (a), b)
#    define readmeml_n(s, a, b)     readmemll_no_mmut_2386((s) + (a), b)
#    define readmemb(s, a)          readmembl_2386((s) + (a))
#    define readmemw(s, a)          readmemwl_2386((s) + (a))
#    define readmeml(s, a)          readmemll_2386((s) + (a))
#    define readmemq(s, a)          readmemql_2386((s) + (a))

#    define writememb_n(s, a, b, v) writemembl_no_mmut_2386((s) + (a), b, v)
#    define writememw_n(s, a, b, v) writememwl_no_mmut_2386((s) + (a), b, v)
#    define writememl_n(s, a, b, v) writememll_no_mmut_2386((s) + (a), b, v)
#    define writememb(s, a, v)      writemembl_2386((s) + (a), v)
#    define writememw(s, a, v)      writememwl_2386((s) + (a), v)
#    define writememl(s, a, v)      writememll_2386((s) + (a), v)
#    define writememq(s, a, v)      writememql_2386((s) + (a), v)

#    define do_mmut_rb(s, a, b)     do_mmutranslate_2386((s) + (a), b, 1, 0)
#    define do_mmut_rw(s, a, b)     do_mmutranslate_2386((s) + (a), b, 2, 0)
#    define do_mmut_rl(s, a, b)     do_mmutranslate_2386((s) + (a), b, 4, 0)
#    define do_mmut_rb2(s, a, b)    do_mmutranslate_2386((s) + (a), b, 1, 0)
#    define do_mmut_rw2(s, a, b)    do_mmutranslate_2386((s) + (a), b, 2, 0)
#    define do_mmut_rl2(s, a, b)    do_mmutranslate_2386((s) + (a), b, 4, 0)

#    define do_mmut_wb(s, a, b)     do_mmutranslate_2386((s) + (a), b, 1, 1)
#    define do_mmut_ww(s, a, b)     do_mmutranslate_2386((s) + (a), b, 2, 1)
#    define do_mmut_wl(s, a, b)     do_mmutranslate_2386((s) + (a), b, 4, 1)
#elif defined(USE_DEBUG_REGS_486)
#    define readmemb_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) ? readmembl_no_mmut((s) + (a), b) : *(uint8_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))
#    define readmemw_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF) || (((s) + (a)) & 1)) ? readmemwl_no_mmut((s) + (a), b) : *(uint16_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmeml_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF) || (((s) + (a)) & 3)) ? readmemll_no_mmut((s) + (a), b) : *(uint32_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmemb(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) ? readmembl((s) + (a)) : *(uint8_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))
#    define readmemw(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF) || (((s) + (a)) & 1)) ? readmemwl((s) + (a)) : *(uint16_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmeml(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF) || (((s) + (a)) & 3)) ? readmemll((s) + (a)) : *(uint32_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmemq(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF) || (((s) + (a)) & 7)) ? readmemql((s) + (a)) : *(uint64_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))

#    define writememb_n(s, a, b, v)                                                                                      \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) \
            writemembl_no_mmut((s) + (a), b, v);                                                                         \
        else                                                                                                             \
            *(uint8_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememw_n(s, a, b, v)                                                                                                                   \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1) || (dr[7] & 0xFF))         \
            writememwl_no_mmut((s) + (a), b, v);                                                                                                      \
        else                                                                                                                                          \
            *(uint16_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememl_n(s, a, b, v)                                                                                                           \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3) || (dr[7] & 0xFF)) \
            writememll_no_mmut((s) + (a), b, v);                                                                                              \
        else                                                                                                                                  \
            *(uint32_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememb(s, a, v)                                                                                           \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) \
            writemembl((s) + (a), v);                                                                                    \
        else                                                                                                             \
            *(uint8_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememw(s, a, v)                                                                                                                \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1) || (dr[7] & 0xFF)) \
            writememwl((s) + (a), v);                                                                                                         \
        else                                                                                                                                  \
            *(uint16_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememl(s, a, v)                                                                                                                \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3) || (dr[7] & 0xFF)) \
            writememll((s) + (a), v);                                                                                                         \
        else                                                                                                                                  \
            *(uint32_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememq(s, a, v)                                                                                                                \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 7) || (dr[7] & 0xFF)) \
            writememql((s) + (a), v);                                                                                                         \
        else                                                                                                                                  \
            *(uint64_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v

#    define do_mmut_rb(s, a, b)                                                                                         \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 1, 0)
#    define do_mmut_rw(s, a, b)                                                                                                              \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 2, 0)
#    define do_mmut_rl(s, a, b)                                                                                                              \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 4, 0)
#    define do_mmut_rb2(s, a, b)                                                      \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);                          \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 1, 0)
#    define do_mmut_rw2(s, a, b)                                                                           \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);                                               \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 2, 0)
#    define do_mmut_rl2(s, a, b)                                                                           \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);                                               \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 4, 0)

#    define do_mmut_wb(s, a, b)                                                                                          \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 1, 1)
#    define do_mmut_ww(s, a, b)                                                                                                               \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 2, 1)
#    define do_mmut_wl(s, a, b)                                                                                                               \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3) || (dr[7] & 0xFF)) \
        do_mmutranslate((s) + (a), b, 4, 1)
#else
#    define readmemb_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) ? readmembl_no_mmut((s) + (a), b) : *(uint8_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))
#    define readmemw_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) ? readmemwl_no_mmut((s) + (a), b) : *(uint16_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmeml_n(s, a, b) ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) ? readmemll_no_mmut((s) + (a), b) : *(uint32_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmemb(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) ? readmembl((s) + (a)) : *(uint8_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))
#    define readmemw(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) ? readmemwl((s) + (a)) : *(uint16_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmeml(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) ? readmemll((s) + (a)) : *(uint32_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uint32_t) ((s) + (a))))
#    define readmemq(s, a)      ((READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 7)) ? readmemql((s) + (a)) : *(uint64_t *) MEM_PTR_FIXUP(READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))))

#    define writememb_n(s, a, b, v)                                                                    \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) \
            writemembl_no_mmut((s) + (a), b, v);                                                       \
        else                                                                                           \
            *(uint8_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememw_n(s, a, b, v)                                                                                         \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) \
            writememwl_no_mmut((s) + (a), b, v);                                                                            \
        else                                                                                                                \
            *(uint16_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememl_n(s, a, b, v)                                                                                         \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) \
            writememll_no_mmut((s) + (a), b, v);                                                                            \
        else                                                                                                                \
            *(uint32_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememb(s, a, v)                                                                         \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) \
            writemembl((s) + (a), v);                                                                  \
        else                                                                                           \
            *(uint8_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememw(s, a, v)                                                                                              \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) \
            writememwl((s) + (a), v);                                                                                       \
        else                                                                                                                \
            *(uint16_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememl(s, a, v)                                                                                              \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) \
            writememll((s) + (a), v);                                                                                       \
        else                                                                                                                \
            *(uint32_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v
#    define writememq(s, a, v)                                                                                              \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 7)) \
            writememql((s) + (a), v);                                                                                       \
        else                                                                                                                \
            *(uint64_t *) MEM_PTR_FIXUP(WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) + (uintptr_t) ((s) + (a))) = v

#    define do_mmut_rb(s, a, b)                                                                       \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) \
        do_mmutranslate((s) + (a), b, 1, 0)
#    define do_mmut_rw(s, a, b)                                                                                            \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) \
        do_mmutranslate((s) + (a), b, 2, 0)
#    define do_mmut_rl(s, a, b)                                                                                            \
        if (READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) \
        do_mmutranslate((s) + (a), b, 4, 0)
#    define do_mmut_rb2(s, a, b)                                    \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);        \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) \
        do_mmutranslate((s) + (a), b, 1, 0)
#    define do_mmut_rw2(s, a, b)                                                         \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);                             \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) \
        do_mmutranslate((s) + (a), b, 2, 0)
#    define do_mmut_rl2(s, a, b)                                                         \
        old_rl2 = READLOOKUP2_GET((uint32_t) ((s) + (a)) >> 12);                             \
        if (old_rl2 == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) \
        do_mmutranslate((s) + (a), b, 4, 0)

#    define do_mmut_wb(s, a, b)                                                                        \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF) \
        do_mmutranslate((s) + (a), b, 1, 1)
#    define do_mmut_ww(s, a, b)                                                                                             \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 1)) \
        do_mmutranslate((s) + (a), b, 2, 1)
#    define do_mmut_wl(s, a, b)                                                                                             \
        if (WRITELOOKUP2_GET((uint32_t) ((s) + (a)) >> 12) == (uintptr_t) LOOKUP_INV || (s) == 0xFFFFFFFF || (((s) + (a)) & 3)) \
        do_mmutranslate((s) + (a), b, 4, 1)
#endif

int checkio(uint32_t port, int mask);

#define check_io_perm(port, size)                                    \
    if (msw & 1 && ((CPL > IOPL) || (cpu_state.eflags & VM_FLAG))) { \
        int tempi = checkio(port, (1 << size) - 1);                  \
        if (cpu_state.abrt)                                          \
            return 1;                                                \
        if (tempi) {                                                 \
            if (cpu_state.eflags & VM_FLAG)                          \
                x86gpf_expected(NULL, 0);                            \
            else                                                     \
                x86gpf(NULL, 0);                                     \
            return 1;                                                \
        }                                                            \
    }

#define SEG_CHECK_READ(seg)                  \
    do {                                     \
        if ((seg)->base == 0xffffffff) {     \
            x86gpf("Segment can't read", 0); \
            return 1;                        \
        }                                    \
    } while (0)

#define SEG_CHECK_WRITE(seg)                  \
    do {                                      \
        if ((seg)->base == 0xffffffff) {      \
            x86gpf("Segment can't write", 0); \
            return 1;                         \
        }                                     \
    } while (0)

#define CHECK_READ(chseg, low, high)                                                                                                                   \
    if ((low < (chseg)->limit_low) || (high > (chseg)->limit_high) || ((msw & 1) && !(cpu_state.eflags & VM_FLAG) && (((chseg)->access & 10) == 8))) { \
        x86gpf("Limit check (READ)", 0);                                                                                                               \
        return 1;                                                                                                                                      \
    }                                                                                                                                                  \
    if (msw & 1 && !(cpu_state.eflags & VM_FLAG) && !((chseg)->access & 0x80)) {                                                                       \
        if ((chseg) == &cpu_state.seg_ss)                                                                                                              \
            x86ss(NULL, (chseg)->seg & 0xfffc);                                                                                                        \
        else                                                                                                                                           \
            x86np("Read from seg not present", (chseg)->seg & 0xfffc);                                                                                 \
        return 1;                                                                                                                                      \
    }

#define CHECK_READ_REP(chseg, low, high)                                         \
    if ((low < (chseg)->limit_low) || (high > (chseg)->limit_high)) {            \
        x86gpf("Limit check (READ)", 0);                                         \
        break;                                                                   \
    }                                                                            \
    if (msw & 1 && !(cpu_state.eflags & VM_FLAG) && !((chseg)->access & 0x80)) { \
        if ((chseg) == &cpu_state.seg_ss)                                        \
            x86ss(NULL, (chseg)->seg & 0xfffc);                                  \
        else                                                                     \
            x86np("Read from seg not present", (chseg)->seg & 0xfffc);           \
        break;                                                                   \
    }

#define CHECK_WRITE_COMMON(chseg, low, high)                                                                                                                             \
    if ((low < (chseg)->limit_low) || (high > (chseg)->limit_high) || !((chseg)->access & 2) || ((msw & 1) && !(cpu_state.eflags & VM_FLAG) && ((chseg)->access & 8))) { \
        x86gpf("Limit check (WRITE)", 0);                                                                                                                                \
        return 1;                                                                                                                                                        \
    }                                                                                                                                                                    \
    if (msw & 1 && !(cpu_state.eflags & VM_FLAG) && !((chseg)->access & 0x80)) {                                                                                         \
        if ((chseg) == &cpu_state.seg_ss)                                                                                                                                \
            x86ss(NULL, (chseg)->seg & 0xfffc);                                                                                                                          \
        else                                                                                                                                                             \
            x86np("Write to seg not present", (chseg)->seg & 0xfffc);                                                                                                    \
        return 1;                                                                                                                                                        \
    }

#define CHECK_WRITE(chseg, low, high) \
    CHECK_WRITE_COMMON(chseg, low, high)

#define CHECK_WRITE_2OP(chseg, low, high, low2, high2)                                                                                                                             \
    if ((low < (chseg)->limit_low) || (high > (chseg)->limit_high) || (low2 < (chseg)->limit_low) || (high2 > (chseg)->limit_high) || !((chseg)->access & 2) || ((msw & 1) && !(cpu_state.eflags & VM_FLAG) && ((chseg)->access & 8))) { \
        x86gpf("Limit check (WRITE)", 0);                                                                                                                                \
        return 1;                                                                                                                                                        \
    }                                                                                                                                                                    \
    if (msw & 1 && !(cpu_state.eflags & VM_FLAG) && !((chseg)->access & 0x80)) {                                                                                         \
        if ((chseg) == &cpu_state.seg_ss)                                                                                                                                \
            x86ss(NULL, (chseg)->seg & 0xfffc);                                                                                                                          \
        else                                                                                                                                                             \
            x86np("Write to seg not present", (chseg)->seg & 0xfffc);                                                                                                    \
        return 1;                                                                                                                                                        \
    }

#define CHECK_WRITE_REP(chseg, low, high)                                        \
    if ((low < (chseg)->limit_low) || (high > (chseg)->limit_high)) {            \
        x86gpf("Limit check (WRITE REP)", 0);                                    \
        break;                                                                   \
    }                                                                            \
    if (msw & 1 && !(cpu_state.eflags & VM_FLAG) && !((chseg)->access & 0x80)) { \
        if ((chseg) == &cpu_state.seg_ss)                                        \
            x86ss(NULL, (chseg)->seg & 0xfffc);                                  \
        else                                                                     \
            x86np("Write (REP) to seg not present", (chseg)->seg & 0xfffc);      \
        break;                                                                   \
    }

#define NOTRM                                         \
    if (!(msw & 1) || (cpu_state.eflags & VM_FLAG)) { \
        x86_int(6);                                   \
        return 1;                                     \
    }

#ifdef OPS_286_386
/* TODO: Introduce functions to read exec. */
static __inline uint8_t
fastreadb(uint32_t a)
{
    uint8_t ret;
    read_type = 1;
    ret = readmembl_2386(a);
    read_type = 4;
    if (cpu_state.abrt)
        return 0;
    return ret;
}

static __inline uint16_t
fastreadw(uint32_t a)
{
    uint16_t ret;
    read_type = 1;
    ret = readmemwl_2386(a);
    read_type = 4;
    if (cpu_state.abrt)
        return 0;
    return ret;
}

static __inline uint32_t
fastreadl(uint32_t a)
{
    uint32_t ret;
    read_type = 1;
    ret = readmemll_2386(a);
    read_type = 4;
    if (cpu_state.abrt)
        return 0;
    return ret;
}
#else
static __inline uint8_t
fastreadb(uint32_t a)
{
    uint8_t *t;

#    ifdef USE_DEBUG_REGS_486
    read_type = 1;
    mem_debug_check_addr(a, read_type);
    read_type = 4;
#    endif

    if (PCCACHE_VALID(a))
        return *((uint8_t *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));

    t = getpccache(a);
    if (cpu_state.abrt)
        return 0;
    pccache  = a >> 12;
    pccache2 = t;

    return *((uint8_t *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));
}

static __inline uint16_t
fastreadw(uint32_t a)
{
    uint8_t *t;
    uint16_t val;
#    ifdef USE_DEBUG_REGS_486
    read_type = 1;
    mem_debug_check_addr(a, read_type);
    mem_debug_check_addr(a + 1, read_type);
    read_type = 4;
#    endif
    if ((a & 0xFFF) > 0xFFE) {
        val = fastreadb(a);
        val |= (fastreadb(a + 1) << 8);
        return val;
    }
    if (PCCACHE_VALID(a))
        return mem_load_u16_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));

    t = getpccache(a);
    if (cpu_state.abrt)
        return 0;

    pccache  = a >> 12;
    pccache2 = t;

    return mem_load_u16_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));
}

static __inline uint32_t
fastreadl(uint32_t a)
{
    uint8_t *t;
    uint32_t val;
#    ifdef USE_DEBUG_REGS_486
    int i;
    read_type = 1;
    for (i = 0; i < 4; i++) {
        mem_debug_check_addr(a + i, read_type);
    }
    read_type = 4;
#    endif
    if ((a & 0xFFF) < 0xFFD) {
        if (!PCCACHE_VALID(a)) {
            t = getpccache(a);
            if (cpu_state.abrt)
                return 0;
            pccache2 = t;
            pccache  = a >> 12;
        }
        
        return mem_load_u32_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));
    }
    val = fastreadw(a);
    val |= (fastreadw(a + 2) << 16);
    return val;
}
#endif

static __inline void *
get_ram_ptr(uint32_t a)
{
    if (PCCACHE_VALID(a))
        return (void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]);
    else {
        uint8_t *t = getpccache(a);
        return (void *) PTR_RECOMBINE(&t[a], &t[0]);
    }
}

extern int opcode_has_modrm[256];
extern int opcode_length[256];

#ifdef OPS_286_386
static __inline uint16_t
fastreadw_fetch(uint32_t a)
{
    uint16_t ret;

    cpu_old_paging = (cpu_flush_pending == 2);
    if ((a & 0xFFF) > 0xFFE) {
        ret = fastreadb(a);
        if (!cpu_state.abrt && (opcode_length[ret & 0xff] > 1))
            ret |= ((uint16_t) fastreadb(a + 1) << 8);
    } else if (cpu_state.abrt)
        ret = 0;
    else {
        read_type = 1;
        ret = readmemwl_2386(a);
        read_type = 4;
    }
    cpu_old_paging = 0;

    return ret;
}

static __inline uint32_t
fastreadl_fetch(uint32_t a)
{
    uint32_t ret;

    if (cpu_16bitbus || ((a & 0xFFF) > 0xFFC)) {
        ret = fastreadw_fetch(a);
        if (!cpu_state.abrt && (opcode_length[ret & 0xff] > 2))
            ret |= ((uint32_t) fastreadw(a + 2) << 16);
    } else if (cpu_state.abrt)
        ret = 0;
    else {
        read_type = 1;
        cpu_old_paging = (cpu_flush_pending == 2);
        ret = readmemll_2386(a);
        cpu_old_paging = 0;
        read_type = 4;
    }

    return ret;
}
#else
static __inline uint16_t
fastreadw_fetch(uint32_t a)
{
    uint8_t *t;
    uint16_t val;
#    ifdef USE_DEBUG_REGS_486
    read_type = 1;
    mem_debug_check_addr(a, read_type);
    mem_debug_check_addr(a + 1, read_type);
    read_type = 4;
#    endif
    if ((a & 0xFFF) > 0xFFE) {
        val = fastreadb(a);
        if (opcode_length[val & 0xff] > 1)
            val |= (fastreadb(a + 1) << 8);
        return val;
    }
    if (PCCACHE_VALID(a))
        return mem_load_u16_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));
    t = getpccache(a);
    if (cpu_state.abrt)
        return 0;

    pccache  = a >> 12;
    pccache2 = t;

    return mem_load_u16_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));

}

static __inline uint32_t
fastreadl_fetch(uint32_t a)
{
    uint8_t *t;
    uint32_t val;
#    ifdef USE_DEBUG_REGS_486
    int i;
    read_type = 1;
    for (i = 0; i < 4; i++) {
        mem_debug_check_addr(a + i, read_type);
    }
    read_type = 4;
#    endif
    if ((a & 0xFFF) < 0xFFD) {
        if (!PCCACHE_VALID(a)) {
            t = getpccache(a);
            if (cpu_state.abrt)
                return 0;
            pccache2 = t;
            pccache  = a >> 12;
        }
#    ifdef CLAUDE_FIX
        /* PCCACHE_VALID only compares the page NUMBER (a >> 12 == pccache);
         * it says nothing about whether pccache2 still points at the buffer
         * that actually backs this page right now. If some chipset path
         * remaps this same page's exec pointer without going through
         * getpccache() again (i.e. without also invalidating pccache), a
         * cache HIT here can still hand back a dangling pointer - this is
         * exactly the deterministic "Load access fault" seen on real
         * hardware at a fixed address/point in POST, unrelated to the
         * opti495 shadow-RAM fix (that fix did not change this crash at
         * all - same fault address, same timing). Cheap (pointer compares
         * only, no MEM_EXEC_LOOKUP call - that was the expensive mistake
         * last time) sanity check against the real ram[] buffer for
         * addresses below the configured RAM size, with self-heal via a
         * forced re-resolve instead of dereferencing garbage. */
        /* Narrowed to conventional RAM below 640K (2026-07-30), matching
         * the sibling check in mem.c's getpccache(): addresses in
         * 0xa0000-0xfffff (video/option-ROM/BIOS shadow candidate range)
         * can be legitimately ROM-backed rather than ram-backed depending
         * on chipset shadow state, and this check has no way to tell
         * "correctly ROM-backed" from "genuinely stale" there - confirmed
         * on real hardware as a false-positive flood (a tight, perfectly
         * normal BIOS loop at 0xf9391/0xf9393, alternating forever,
         * logged as "stale" on every single iteration even though nothing
         * was actually wrong). */
        {
            uint8_t *ptr = (uint8_t *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]);
            if (a < 0xa0000UL) {
                uint8_t *ram_lo = ram;
                uint8_t *ram_hi = ram + ((uint32_t) mem_size << 10);
                if ((ptr < ram_lo) || (ptr >= ram_hi)) {
                    /* Rate-limited (2026-07-30): a real BIOS shadow-RAM
                     * toggle loop hits this on every single iteration -
                     * legitimately re-resolves correctly each time (no
                     * crash), but logging every hit floods the console to
                     * the point of needing a manual stop. Log only the
                     * first few, keep self-healing silently after that. */
                    static uint32_t stale_log_count = 0;
                    if (stale_log_count < 5) {
                        stale_log_count++;
                        pclog("# fastreadl_fetch: stale pccache2 for a=%08X pccache=%08X ptr=%p ram=%p ram_hi=%p - re-resolving (log %u/5, further hits silent)\n",
                              a, pccache, (void *) ptr, (void *) ram_lo, (void *) ram_hi, (unsigned) stale_log_count);
                    }
                    t = getpccache(a);
                    if (cpu_state.abrt)
                        return 0;
                    pccache2 = t;
                    pccache  = a >> 12;
                    ptr      = (uint8_t *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]);
                }
            }
            return mem_load_u32_unaligned(ptr);
        }
#    elif (defined __amd64__ || defined _M_X64 || defined __aarch64__ || defined _M_ARM64 || (defined(__riscv) && (__SIZEOF_POINTER__ == 8)))
        return mem_load_u32_unaligned((void *) PTR_RECOMBINE(&pccache2[a], &pccache2[0]));
#    else
        return mem_load_u32_unaligned(&pccache2[a]);
#    endif
    }
    val = fastreadw_fetch(a);
    if (opcode_length[val & 0xff] > 2)
        val |= (fastreadw(a + 2) << 16);
    return val;
}
#endif

#ifdef OPS_286_386
static __inline uint8_t
getbyte(void)
{
    uint8_t ret;
    cpu_state.pc++;
    cpu_old_paging = (cpu_flush_pending == 2);
    ret = fastreadb(cs + (cpu_state.pc - 1));
    cpu_old_paging = 0;
    return ret;

}

static __inline uint16_t
getword(void)
{
    uint16_t ret;
    cpu_state.pc += 2;
    cpu_old_paging = (cpu_flush_pending == 2);
    ret = fastreadw(cs + (cpu_state.pc - 2));
    cpu_old_paging = 0;
    return ret;
}

static __inline uint32_t
getlong(void)
{
    uint32_t ret;
    cpu_state.pc += 4;
    cpu_old_paging = (cpu_flush_pending == 2);
    ret = fastreadl(cs + (cpu_state.pc - 4));
    cpu_old_paging = 0;
    return ret;
}

static __inline uint64_t
getquad(void)
{
    uint64_t ret;
    cpu_state.pc += 8;
    cpu_old_paging = (cpu_flush_pending == 2);
    ret = fastreadl(cs + (cpu_state.pc - 8)) | ((uint64_t) fastreadl(cs + (cpu_state.pc - 4)) << 32);
    cpu_old_paging = 0;
    return ret;
}

static __inline uint8_t
geteab(void)
{
    if (cpu_mod == 3)
        return (cpu_rm & 4) ? cpu_state.regs[cpu_rm & 3].b.h : cpu_state.regs[cpu_rm & 3].b.l;
    return readmemb(easeg, cpu_state.eaaddr);
}

static __inline uint16_t
geteaw(void)
{
    if (cpu_mod == 3)
        return cpu_state.regs[cpu_rm].w;
    return readmemw(easeg, cpu_state.eaaddr);
}

static __inline uint32_t
geteal(void)
{
    if (cpu_mod == 3)
        return cpu_state.regs[cpu_rm].l;
    return readmeml(easeg, cpu_state.eaaddr);
}

static __inline uint64_t
geteaq(void)
{
    return readmemq(easeg, cpu_state.eaaddr);
}

static __inline uint8_t
geteab_mem(void)
{
    return readmemb(easeg, cpu_state.eaaddr);
}
static __inline uint16_t
geteaw_mem(void)
{
    return readmemw(easeg, cpu_state.eaaddr);
}
static __inline uint32_t
geteal_mem(void)
{
    return readmeml(easeg, cpu_state.eaaddr);
}

static __inline int
seteaq_cwc(void)
{
    CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr);
    return 0;
}

static __inline void
seteaq(uint64_t v)
{
    if (seteaq_cwc())
        return;
    writememql(easeg + cpu_state.eaaddr, v);
}

#    define seteab(v)                                                                 \
        if (cpu_mod != 3) {                                                           \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr); \
            writemembl_2386(easeg + cpu_state.eaaddr, v);                             \
        } else if (cpu_rm & 4)                                                        \
            cpu_state.regs[cpu_rm & 3].b.h = v;                                       \
        else                                                                          \
            cpu_state.regs[cpu_rm].b.l = v
#    define seteaw(v)                                                                     \
        if (cpu_mod != 3) {                                                               \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 1); \
            writememwl_2386(easeg + cpu_state.eaaddr, v);                                 \
        } else                                                                            \
            cpu_state.regs[cpu_rm].w = v
#    define seteal(v)                                                                     \
        if (cpu_mod != 3) {                                                               \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 3); \
            writememll_2386(easeg + cpu_state.eaaddr, v);                                 \
        } else                                                                            \
            cpu_state.regs[cpu_rm].l = v

#    define seteab_mem(v) writemembl_2386(easeg + cpu_state.eaaddr, v);
#    define seteaw_mem(v) writememwl_2386(easeg + cpu_state.eaaddr, v);
#    define seteal_mem(v) writememll_2386(easeg + cpu_state.eaaddr, v);
#else
static __inline uint8_t
getbyte(void)
{
    cpu_state.pc++;
    return fastreadb(cs + (cpu_state.pc - 1));
}

static __inline uint16_t
getword(void)
{
    cpu_state.pc += 2;
    return fastreadw(cs + (cpu_state.pc - 2));
}

static __inline uint32_t
getlong(void)
{
    cpu_state.pc += 4;
    return fastreadl(cs + (cpu_state.pc - 4));
}

static __inline uint64_t
getquad(void)
{
    cpu_state.pc += 8;
    return fastreadl(cs + (cpu_state.pc - 8)) | ((uint64_t) fastreadl(cs + (cpu_state.pc - 4)) << 32);
}

static __inline uint8_t
geteab(void)
{
    if (cpu_mod == 3)
        return (cpu_rm & 4) ? cpu_state.regs[cpu_rm & 3].b.h : cpu_state.regs[cpu_rm & 3].b.l;
    if (eal_r)
        return *(uint8_t *) eal_r;
    return readmemb(easeg, cpu_state.eaaddr);
}

static __inline uint16_t
geteaw(void)
{
    if (cpu_mod == 3)
        return cpu_state.regs[cpu_rm].w;
    if (eal_r)
        return *(uint16_t *) eal_r;
    return readmemw(easeg, cpu_state.eaaddr);
}

static __inline uint32_t
geteal(void)
{
    if (cpu_mod == 3)
        return cpu_state.regs[cpu_rm].l;
    if (eal_r)
        return *eal_r;
    return readmeml(easeg, cpu_state.eaaddr);
}

static __inline uint64_t
geteaq(void)
{
    return readmemq(easeg, cpu_state.eaaddr);
}

static __inline uint8_t
geteab_mem(void)
{
    if (eal_r)
        return *(uint8_t *) eal_r;
    return readmemb(easeg, cpu_state.eaaddr);
}
static __inline uint16_t
geteaw_mem(void)
{
    if (eal_r)
        return *(uint16_t *) eal_r;
    return readmemw(easeg, cpu_state.eaaddr);
}
static __inline uint32_t
geteal_mem(void)
{
    if (eal_r)
        return *eal_r;
    return readmeml(easeg, cpu_state.eaaddr);
}

static __inline int
seteaq_cwc(void)
{
    CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr);
    return 0;
}

static __inline void
seteaq(uint64_t v)
{
    if (seteaq_cwc())
        return;
    writememql(easeg + cpu_state.eaaddr, v);
}

#    define seteab(v)                                                                 \
        if (cpu_mod != 3) {                                                           \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr); \
            if (eal_w)                                                                \
                *(uint8_t *) eal_w = v;                                               \
            else                                                                      \
                writemembl(easeg + cpu_state.eaaddr, v);                              \
        } else if (cpu_rm & 4)                                                        \
            cpu_state.regs[cpu_rm & 3].b.h = v;                                       \
        else                                                                          \
            cpu_state.regs[cpu_rm].b.l = v
#    define seteaw(v)                                                                     \
        if (cpu_mod != 3) {                                                               \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 1); \
            if (eal_w)                                                                    \
                *(uint16_t *) eal_w = v;                                                  \
            else                                                                          \
                writememwl(easeg + cpu_state.eaaddr, v);                                  \
        } else                                                                            \
            cpu_state.regs[cpu_rm].w = v
#    define seteal(v)                                                                     \
        if (cpu_mod != 3) {                                                               \
            CHECK_WRITE_COMMON(cpu_state.ea_seg, cpu_state.eaaddr, cpu_state.eaaddr + 3); \
            if (eal_w)                                                                    \
                *eal_w = v;                                                               \
            else                                                                          \
                writememll(easeg + cpu_state.eaaddr, v);                                  \
        } else                                                                            \
            cpu_state.regs[cpu_rm].l = v

#    define seteab_mem(v)           \
        if (eal_w)                  \
            *(uint8_t *) eal_w = v; \
        else                        \
            writemembl(easeg + cpu_state.eaaddr, v);
#    define seteaw_mem(v)            \
        if (eal_w)                   \
            *(uint16_t *) eal_w = v; \
        else                         \
            writememwl(easeg + cpu_state.eaaddr, v);
#    define seteal_mem(v) \
        if (eal_w)        \
            *eal_w = v;   \
        else              \
            writememll(easeg + cpu_state.eaaddr, v);
#endif

#define getbytef()          \
    ((uint8_t) (fetchdat)); \
    cpu_state.pc++
#define getwordf()           \
    ((uint16_t) (fetchdat)); \
    cpu_state.pc += 2
#define getbyte2f()              \
    ((uint8_t) (fetchdat >> 8)); \
    cpu_state.pc++
#define getword2f()               \
    ((uint16_t) (fetchdat >> 8)); \
    cpu_state.pc += 2

#endif

/* Resume Flag handling. */
extern int rf_flag_no_clear;

int cpu_386_check_instruction_fault(void);
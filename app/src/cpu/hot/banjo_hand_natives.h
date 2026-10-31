// banjo-nx - hand-written native versions of hot guest functions (CPU enhancements banjo_hand_*).
//
// The localized copies of tools/localize_native.py keep a function's own statements; these replace the statements:
// the XDK C runtime's memcpy and memset and Direct3D's two shader float-constant writers, written from what the
// routines do. They are sa2-nx's natives (app/src/cpu/hot/n_827F05C0.h, n_827EFEA0.h, n_d3d_constants.h, GPL-3.0;
// the memcpy came there from masseffect-nx's n_82AC4AF0.h): tools/crt_signatures.py of sa2-nx finds the same three C
// runtime routines in Banjo's image (unique matches), and the two constant writers are the same instructions with
// the same two offsets. Banjo's own additions are the two writers of Direct3D's state flush (namespace flush below).
// What is different here is how they are used and checked:
//
//   * Each is a template on the store policy of banjo_hot_common.h, so the guard's logged form records what it is
//     about to write like any generated copy (no hand-written list of written ranges).
//   * tools/localize_native.py puts them IN FRONT of the generated copy of the same function wherever that copy is
//     used: as a hook of its own, and inside every other function's copy that contains it (app/src/cpu/hot/hand.txt).
//     Inside a caller's copy the call is inlined: no hook, no hand-over of every register (on the Switch that
//     hand-over is what made copies with real calls slower than their originals).
//   * A native returns false to DECLINE a call it does not cover (nothing written yet): the generated copy of the
//     original then runs, in the same place.
//   * The registers the original leaves behind as scratch (hand.txt gives them per function; tools/localize_native.py
//     takes them from the original's own statements) are NOT reproduced: the natives write no register. Those
//     registers are left out of the guard's register compare for every hook whose copy contains the native. Sound
//     because they are caller-saved scratch of assembly routines and of a callee the callers reach through the
//     normal calling convention: a caller that read one after the call would get a different value, would act on
//     it, and the guard would see that in everything else it compares.
//
// ARM (the A57 is where this has to pay, and sa2-nx's first native Direct3D layer lost there for reasons no PC run
// showed: docs/api-renderer.md of sa2-nx, section 6): nothing here is per thread (no thread_local: a function call on
// the Switch), nothing copies a context, no loop keeps a count where a store could alias it, nothing is byte-swapped
// word by word (the copies are guest order to guest order), and sizes up to 128 bytes (memcpy; 64 for memset) are
// moved with overlapping 16/8/4/2-byte loads and stores without a call into the C library. In a PC run of title,
// menus, intro and gameplay 62 % of the 21.9 million memcpy calls were under 64 bytes and 21 % were 64 to 127; 46 %
// of the memsets were 4 to 7 bytes; 98 % of the 26 million constant uploads were 1 to 7 vectors.
//
// Guest function addresses may be named in this folder only (tools/direct_calls.py and tools/localize_native.py
// read every other source's addresses as hooked functions; these four are hooked, by app/src/cpu/hot/localized.txt).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include <rex/ppc/context.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#include "cpu/banjo_hot_common.h"
#include <rex/ppc/diet_ops.h>  // katamari-nx: rex::ppc::SingleToDoublePure

// katamari-nx: the in-run A/B's target in its off half (renut_engine/gpu/banjo_renut_tuning.cpp).
#include <atomic>
namespace banjo::renut_gpu {
extern std::atomic<int> g_ab_off;
}  // namespace banjo::renut_gpu

namespace banjo::hand {

// The natives' own keys (banjo_hot_guest.cpp), each read once: 0 not read yet, 1 off, 2 on.
// katamari-nx: kMatCopy / kMatMul / kMatTranslation are Beautiful Katamari's (the end of this file); kMemset serves its
// C runtime memset too. N&B's other natives stay for reference: hand.txt does not list them here, so they never compile.
enum : uint32_t {
  kMemcpy = 0,
  kMemset = 1,
  kConstants = 2,
  kFlush = 3,
  kAudioFilter = 4,
  kXAudioMix = 5,
  kMatCopy = 6,
  kMatMul = 7,
  kMatTranslation = 8,
  kMatIdentity = 9,
  kMatCompare = 10,
  kCopyWords = 11,
  kStringHash = 12,
  kVec4Update = 13,
  kMatProduct = 14,
  kVecSet = 15,
  kSphereBox = 16,
  kRotate = 17,
  kTransform = 18,
  kIntegrate = 19,
  kSine = 20,
  kNatives = 21
};
extern uint8_t g_state[kNatives];
uint8_t Init(uint32_t which);
[[gnu::always_inline]] inline bool On(uint32_t which) {
  const uint8_t state = g_state[which];
  if (state == 2) [[likely]] return true;
  return state == 0 && Init(which) == 2;
}

namespace detail {

// 16 bytes through a register (any alignment; one NEON load or store on the A57).
struct B16 {
  uint8_t b[16];
};
[[gnu::always_inline]] inline B16 L16(const uint8_t* p) {
  B16 v;
  std::memcpy(v.b, p, 16);
  return v;
}
[[gnu::always_inline]] inline void S16(uint8_t* p, const B16& v) { std::memcpy(p, v.b, 16); }

// 1 <= n <= 64; all loads before all stores (memmove semantics for dst <= src overlap).
[[gnu::always_inline]] inline void CopySmall(uint8_t* d, const uint8_t* s, uint32_t n) {
  if (n >= 16) {
    if (n > 32) {
      const B16 a = L16(s), b = L16(s + 16), c = L16(s + n - 32), e = L16(s + n - 16);
      S16(d, a);
      S16(d + 16, b);
      S16(d + n - 32, c);
      S16(d + n - 16, e);
    } else {
      const B16 a = L16(s), e = L16(s + n - 16);
      S16(d, a);
      S16(d + n - 16, e);
    }
  } else if (n >= 8) {
    uint64_t a, e;
    std::memcpy(&a, s, 8);
    std::memcpy(&e, s + n - 8, 8);
    std::memcpy(d, &a, 8);
    std::memcpy(d + n - 8, &e, 8);
  } else if (n >= 4) {
    uint32_t a, e;
    std::memcpy(&a, s, 4);
    std::memcpy(&e, s + n - 4, 4);
    std::memcpy(d, &a, 4);
    std::memcpy(d + n - 4, &e, 4);
  } else if (n >= 2) {
    uint16_t a, e;
    std::memcpy(&a, s, 2);
    std::memcpy(&e, s + n - 2, 2);
    std::memcpy(d, &a, 2);
    std::memcpy(d + n - 2, &e, 2);
  } else {
    *d = *s;
  }
}

// 64 < n <= 128; all loads before all stores. (Banjo's calls: a fifth are 64 to 127 bytes.)
[[gnu::always_inline]] inline void CopyMedium(uint8_t* d, const uint8_t* s, uint32_t n) {
  const B16 a = L16(s), b = L16(s + 16), c = L16(s + 32), e = L16(s + 48);
  const B16 f = L16(s + n - 64), g = L16(s + n - 48), h = L16(s + n - 32), i = L16(s + n - 16);
  S16(d, a);
  S16(d + 16, b);
  S16(d + 32, c);
  S16(d + 48, e);
  S16(d + n - 64, f);
  S16(d + n - 48, g);
  S16(d + n - 32, h);
  S16(d + n - 16, i);
}

// 1 <= n <= 64, every byte = v.
[[gnu::always_inline]] inline void FillSmall(uint8_t* d, uint8_t v, uint32_t n) {
  if (n >= 16) {
    B16 x;
    std::memset(x.b, v, 16);
    S16(d, x);
    S16(d + n - 16, x);
    if (n > 32) {
      S16(d + 16, x);
      S16(d + n - 32, x);
    }
  } else {
    const uint64_t x = uint64_t(v) * 0x0101010101010101ull;
    if (n >= 8) {
      std::memcpy(d, &x, 8);
      std::memcpy(d + n - 8, &x, 8);
    } else if (n >= 4) {
      std::memcpy(d, &x, 4);
      std::memcpy(d + n - 4, &x, 4);
    } else if (n >= 2) {
      std::memcpy(d, &x, 2);
      std::memcpy(d + n - 2, &x, 2);
    } else {
      *d = v;
    }
  }
}

// [a, a + n) is one stretch of host memory: it does not wrap around 4 GB and (on hosts with the physical-view
// offset) does not cross its boundary. n >= 1.
[[gnu::always_inline]] inline bool Straight(uint32_t a, uint32_t n) {
  if (uint64_t(a) + n > 0x100000000ull) return false;
  return ::banjo::hot::PhysOff(a) == ::banjo::hot::PhysOff(a + n - 1u);
}

}  // namespace detail

// sub_82C111C0 - XDK C runtime memcpy: r3 = dst, r4 = src, r5 = n (its 32-bit part); returns r3 as it came.
//
// The original stores r3 at -8(r1) first (a real store: it reloads r3 from there at every return), then copies
// FORWARD: a byte head up to 8-byte dst alignment, then 128-byte blocks / doublewords / words / bytes chosen by the
// alignment of src; dcbt / dcbtst are hints. Facts used:
//   * every size and address is the 32-bit part (cmplw, rlwinm, ea = u32 + u32);
//   * no loop reads past src + n or writes past dst + n;
//   * within each unit all loads precede its stores and units run upward: for dst <= src the result is memmove's,
//     for ranges that do not overlap a plain copy. For dst > src with dst - src < n the forward copy smears bytes it
//     already copied: DECLINED;
//   * n == 0 copies nothing (the save of r3 still happens).
// Also declined: ranges that wrap around 4 GB, and a destination that covers the save slot (the original would then
// return what was copied over it).
// Scratch the original leaves and this does not reproduce (hand.txt): r0, r4-r12.
namespace n_82C111C0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!On(kMemcpy)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32, n = ctx.r5.u32, slot = ctx.r1.u32 - 8u;
  s.Hand(kMemcpy, n);
  if (n) {
    if (uint32_t(dst - src - 1u) < n - 1u) [[unlikely]] return false;  // dst > src, dst - src < n
    if (!detail::Straight(dst, n) || !detail::Straight(src, n)) [[unlikely]] return false;
    if (uint64_t(uint32_t(slot + 7u - dst)) < uint64_t(n) + 7u) [[unlikely]] return false;  // dst covers the slot
  }
  if (!s.Note(slot, 8)) [[unlikely]] return true;
  const uint64_t saved = __builtin_bswap64(ctx.r3.u64);
  std::memcpy(::banjo::hot::Raw(base, slot), &saved, 8);
  if (n == 0) return true;
  if (!s.Note(dst, n)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, n);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  const uint8_t* p = ::banjo::hot::Raw(base, src);
  if (n <= 64) {
    detail::CopySmall(d, p, n);
  } else if (n <= 128) {
    detail::CopyMedium(d, p, n);
  } else if (uint32_t(src - dst) < n) {
    std::memmove(d, p, n);  // dst < src (or equal), overlapping: the forward copy is memmove
  } else {
    std::memcpy(d, p, n);
  }
  return true;
}

}  // namespace n_82C111C0

// sub_82C116F0 - XDK C runtime memset: r3 = dst, r4 = value (its low byte), r5 = n; r3 is not written.
//
// The original (ori r6,r3,0; ctr = n + 1): single bytes until r6 is 4-aligned or n is used up, the low byte of r4
// replicated into a word, 16-byte blocks of four stw, up to three stw, up to three stb. Every store writes that byte,
// each byte of [dst, dst + n) exactly once, nothing outside, and nothing is read. Facts used:
//   * ctr = r5 + 1 is 64-bit: with anything in the upper half of r5 the head loop could run past n: DECLINED;
//   * ranges that wrap around 4 GB are declined.
// Scratch not reproduced (hand.txt): r0, r4, r5, r6.
namespace n_82C116F0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!On(kMemset)) [[unlikely]] return false;
  if (ctx.r5.u64 >> 32) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, n = ctx.r5.u32;
  s.Hand(kMemset, n);
  if (n == 0) return true;
  if (!detail::Straight(dst, n)) [[unlikely]] return false;
  if (!s.Note(dst, n)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, n);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  if (n <= 64) {
    detail::FillSmall(d, ctx.r4.u8, n);
  } else {
    std::memset(d, int(ctx.r4.u8), n);
  }
  return true;
}

}  // namespace n_82C116F0

// Direct3D's two shader float-constant writers (called for every constant upload of every draw):
//   sub_821FE108: r3 = device, r4 = first constant, r5 = source (any alignment), r6 = count of 16-byte vectors,
//                 r7 = 64-bit dirty bits. Destination ((r3 + (r4 + 120) * 16) & ~15) + 16 k; dirty mask at r3 + 0.
//   sub_821FE230: the same with (r4 + 376) and the dirty mask at r3 + 8.
// The originals copy four vectors per iteration, then one per iteration: lvlx + lvrx + vor is an unaligned 16-byte
// load (lvrx at an aligned address loads nothing), stvx128 an aligned store, dcbt are hints; then ld / or r7 / std on
// the dirty mask. Facts used:
//   * every address is the 32-bit part. The counters here are 32-bit compares (addic. / cmplwi), but r9 = r6 is
//     decremented as 64 bits: a count with anything in its upper half is DECLINED;
//   * the four-at-a-time loop loads its four vectors before it stores them: ranges that overlap are DECLINED, as are
//     ranges that wrap around 4 GB;
//   * the dirty mask is read after the copy, in the same order here;
//   * the base game's originals saved r31 at -8(r1) first (eight zero bytes, the local r31); title update 3's
//     compile of them saves nothing (count loop on ctr, r8 for the third offset), so nothing is written there.
// Scratch not reproduced (hand.txt): r4, r5, r6, r8-r11, v0, v10-v13.
namespace constants {

template <class S>
[[gnu::always_inline]] inline bool Upload(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s,
                                          uint32_t first, uint32_t mask_offset) {
  if (!On(kConstants)) [[unlikely]] return false;
  if (ctx.r6.u64 >> 32) [[unlikely]] return false;
  const uint32_t count = ctx.r6.u32, src = ctx.r5.u32, mask = ctx.r3.u32 + mask_offset;
  const uint32_t dst = (((ctx.r4.u32 + first) << 4) + ctx.r3.u32) & ~15u;
  s.Hand(kConstants, count);
  if (mask > 0xFFFFFFF7u) [[unlikely]] return false;
  if (count > 0x0FFFFFFFu) [[unlikely]] return false;
  const uint32_t bytes = count << 4;
  if (count) {
    if (!detail::Straight(dst, bytes) || !detail::Straight(src, bytes)) [[unlikely]] return false;
    if (uint32_t(src - dst) < bytes || uint32_t(dst - src) < bytes) [[unlikely]] return false;
  }
  // (Title update 3, banjo-nx 2026-10-29: its writers save no register, so the base's eight zero bytes at r1 - 8 are
  // not written; the guard found them as the one difference of the PC run's 64-call checks.)
  if (count) {
    if (!s.Note(dst, bytes)) [[unlikely]] return true;
    ::banjo::hot::Card(base, dst, bytes);
    uint8_t* d = ::banjo::hot::Raw(base, dst);
    const uint8_t* p = ::banjo::hot::Raw(base, src);
    if (count <= 8) {
      const uint8_t* const end = p + bytes;  // (a pointer cursor: no store here can alias the loop's bound)
      do {
        detail::S16(d, detail::L16(p));
        d += 16;
        p += 16;
      } while (p != end);
    } else {
      std::memcpy(d, p, bytes);
    }
  }
  if (!s.Note(mask, 8)) [[unlikely]] return true;
  ::banjo::hot::Card(base, mask, 8);
  uint64_t m;
  std::memcpy(&m, ::banjo::hot::Raw(base, mask), 8);
  m |= __builtin_bswap64(ctx.r7.u64);
  std::memcpy(::banjo::hot::Raw(base, mask), &m, 8);
  return true;
}

}  // namespace constants

namespace n_821FE108 {
template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return constants::Upload(ctx, base, s, 120, 0);
}
}  // namespace n_821FE108

namespace n_821FE230 {
template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return constants::Upload(ctx, base, s, 376, 8);
}
}  // namespace n_821FE230

// Direct3D's state flush, the two functions that turn dirty bits into PM4 packets in the command buffer (called by
// the flush of every draw, once per dirty block of state):
//   sub_821FBC70: r3 = device, r4 = 64 dirty bits (bit 63 first), r5 = register number of the first bit, r6 = the
//                 shadow words (word i belongs to bit i). For every run of set bits: a type-0 header
//                 ((length - 1) << 16 | register) and the run's words, appended after the write pointer at device + 48
//                 (which names the last word written); the new pointer is stored at the end.
//   sub_821FC1C8: the same for the float constants, one bit per block of four vectors (16 registers, 64 bytes): the
//                 run is first padded with type-2 packets (0x80000000) so that its vectors land 16-aligned (the
//                 original stores them with stvx), then header ((16 * blocks - 1) << 16 | register) and the blocks.
// Facts used:
//   * both reload the buffer's limit (device + 52) for every run and call Direct3D's "more space" function when a
//     run does not fit: a call in which a run could fail is DECLINED before anything is written (Runs below);
//   * no dirty bit at all is declined (the originals would then write a run of length 0 and loop);
//   * words go from guest memory to guest memory unchanged: no byte order is touched;
//   * the constants' source must be 16-aligned and the write pointer a multiple of 4 (the vector loads and stores
//     round their addresses down): otherwise declined;
//   * the buffer between the write pointer and its limit must not overlap the shadow words or the device's two
//     pointer words (declined), and no address may be near the top of the 4 GB;
//   * the originals' prologue links a stack frame (stwu r1,-144 / -176): the old stack pointer is stored at the new
//     one unless the build elides the back chain (REX_BACK_CHAIN_ELIDE, off in the shipped build). Their saves of the
//     non-volatile registers are not stores in the generated code (those registers are locals of the function).
// Scratch not reproduced (hand.txt): r4-r12, and v0, v11-v13 for the constants. r1 and r3 end as they came.
namespace flush {

[[gnu::always_inline]] inline uint32_t Load32(uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, ::banjo::hot::Raw(base, a), 4);
  return __builtin_bswap32(v);
}
[[gnu::always_inline]] inline void Store32(uint8_t* p, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(p, &v, 4);
}
// [a, a + n) and [b, b + m) share a byte (n, m >= 1; neither wraps).
[[gnu::always_inline]] inline bool Overlap(uint32_t a, uint32_t n, uint32_t b, uint32_t m) {
  return uint64_t(a) < uint64_t(b) + m && uint64_t(b) < uint64_t(a) + n;
}

// kUnit: bytes per dirty bit (4: register words, 64: constant blocks). kFrame: the original's frame size.
//
// One pass, no register saved for it, and only the tests exactness needs. (The first version checked every run in a
// pass of its own and compared the exact ranges: 131 A57 instructions for a call with one short run, where the
// recompiled original takes 152 on average. A second, in one pass but with every pair of ranges tested, still took
// 119. Measured with tools/hot_arm_compare.py and the Switch build's disassembly; none of it showed on the PC.)
//   * the runs are written into the window between the write pointer and the buffer's limit, so that window as a
//     whole is what must not touch the 64 shadow units, the device's two pointer words or the frame word;
//   * a run that does not fit is where the original calls for more space. Whether any run could fail is decided
//     up front from the number of dirty bits alone: n bits make at most n runs, each at most 16 bytes of padding and
//     header besides its words, so with start + 16 n + (bytes of n units) + 12 below the limit every run passes the
//     original's own test and none of them is tested again. Otherwise the call is DECLINED before anything is
//     written (about 1 call in 1000 in a PC run; the original runs, with its call for space). (Handing the rest of
//     the bits to the original at the first run that fails is also exact, but those calls reach a call the guard
//     cannot repeat, so that path would have run unchecked.)
template <class S, uint32_t kUnit, uint32_t kFrame>
[[gnu::always_inline]] inline bool Runs(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!On(kFlush)) [[unlikely]] return false;
  uint64_t m = ctx.r4.u64;
  if (m == 0) [[unlikely]] return false;
  const uint32_t device = ctx.r3.u32, reg0 = ctx.r5.u32, src = ctx.r6.u32, frame = ctx.r1.u32 - kFrame;
  constexpr uint32_t kShadow = 64u * kUnit;  // the shadow bytes 64 dirty bits can name (256 or 4096)
  // No address near the top of the 4 GB, so no sum below wraps.
  if (device > 0xFFFFE000u || frame > 0xFFFFE000u || src > 0xFFFFE000u) [[unlikely]] return false;
  if (kUnit == 64 && (src & 15u)) [[unlikely]] return false;
  // The original's first store, the frame's back chain, comes before it reads anything. Made here before the checks
  // that need the buffer's pointers: should one of them decline, the original starts by storing the same word.
#if !defined(REX_BACK_CHAIN_ELIDE)
  if (!s.Note(frame, 4)) [[unlikely]] return true;
  Store32(::banjo::hot::Raw(base, frame), ctx.r1.u32);  // stwu r1,-frame(r1)
#endif
  const uint32_t start = Load32(base, device + 48), limit = Load32(base, device + 52);
  constexpr uint32_t kWordsPerBit = kUnit / 4;
  const uint32_t bits = uint32_t(__builtin_popcountll(m));
  // Every run fits as the original judges it (above); this also says limit > start + 12.
  if (uint64_t(start) + 16u * bits + 4u * kWordsPerBit * bits + 12u >= limit) [[unlikely]] return false;
  if (limit > 0xFFFFFFE0u || (kUnit == 64 && (start & 3u))) [[unlikely]] return false;
  // Every word written lies in [start + 4, limit + 8): a run is only written when its last word is below limit.
  // That window must not touch the shadow units (they are read while it is written) nor the device's two pointer
  // words (the original reads the limit again for every run and stores the pointer at the end). The frame word
  // needs no test: it is written before everything else, here as there.
  const uint32_t window = limit - start + 8u;
  if (Overlap(start, window, src, kShadow) || Overlap(start, window, device + 48, 8)) [[unlikely]] return false;
  if (::banjo::hot::PhysOff(start) != ::banjo::hot::PhysOff(limit + 7u) ||
      ::banjo::hot::PhysOff(src) != ::banjo::hot::PhysOff(src + kShadow - 1u))
    [[unlikely]] return false;
  s.Hand(kFlush, bits);
  uint8_t* const buffer = ::banjo::hot::Raw(base, start);  // the word the write pointer names
  const uint8_t* const shadow = ::banjo::hot::Raw(base, src);
  uint32_t wp = start;
  uint32_t bit = 0;  // the dirty bit the top of m stands for
  do {
    const uint32_t zeros = uint32_t(__builtin_clzll(m));
    m <<= zeros;
    bit += zeros;
    const uint32_t length = ~m ? uint32_t(__builtin_clzll(~m)) : 64u;
    const uint32_t words = length * kWordsPerBit, bytes = words * 4u;
    // (sub_821FBC70's test, wp + 4 * length < limit, and sub_821FC1C8's, wp + 4 * (16 * blocks + 3) < limit, hold
    // for every run: decided above.)
    uint32_t pads = 0;
    if (kUnit == 64) {
      const uint32_t at = wp & 15u;  // 8: the header lands on 12 already
      pads = at == 4 ? 1u : at == 0 ? 2u : at == 12 ? 3u : 0u;
    }
    const uint32_t run_bytes = 4u * pads + 4u + bytes;
    if (!s.Note(wp + 4u, run_bytes)) [[unlikely]] return true;
    ::banjo::hot::Card(base, wp + 4u, run_bytes);
    uint8_t* d = buffer + (wp - start) + 4;
    for (uint32_t k = 0; k < pads; ++k) {
      Store32(d, 0x80000000u);
      d += 4;
    }
    Store32(d, (((words - 1u) << 16) & 0xFFFF0000u) | (reg0 + bit * kWordsPerBit));
    d += 4;
    const uint8_t* p = shadow + size_t(bit) * kUnit;
    if (bytes <= 64) {
      detail::CopySmall(d, p, bytes);
    } else {
      std::memcpy(d, p, bytes);
    }
    wp += run_bytes;
    bit += length;
    m = length == 64 ? 0 : m << length;
  } while (m);
  if (!s.Note(device + 48, 4)) [[unlikely]] return true;
  ::banjo::hot::Card(base, device + 48, 4);
  Store32(::banjo::hot::Raw(base, device + 48), wp);
  return true;
}

}  // namespace flush

namespace n_821FBC70 {
template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return flush::Runs<S, 4, 144>(ctx, base, s);
}
}  // namespace n_821FBC70

namespace n_821FC1C8 {
template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return flush::Runs<S, 64, 176>(ctx, base, s);
}
}  // namespace n_821FC1C8

// XAudio's per-voice state-variable filter, sub_82B6B258 (banjo-nx 2026-10-23, the user's request: hand-optimise the
// audio thread; the hottest guest function of that thread, a fifth of it in the console's city profile).
//   r3 = the filter, r4 = the voice's frame buffer. It calls XAudioFrameBuffer::GetProcessingData (0x8264E7B0), which
//   copies the buffer's words +12, +16, +20 to its stack frame (+80): byte +81 is the channel count, +88 the samples
//   (256 floats a channel, a channel every 1024 bytes). With byte filter+24 set, the six channels' state (low at
//   +32, band at +56, a float each) is zeroed first. Then for each channel and sample, with q at +28, frequency at
//   +80 and the mode at +20 (> 0 high-pass, < 0 low-pass, 0 band-pass):
//     d = x - low; high = -(fma(q, band, -d)); band += freq * high; low += freq * band; x = the mode's output
//   each a float result of a double multiply-add (as the generated code computes fnmsubs / fmadds).
// The original stores band, low and the sample at every sample and reloads q, the frequency, the mode and the buffer
// pointer every time, each a byte-swapped guest access (nothing it writes can alias them: declined otherwise). Here
// the state stays in registers and the channels run side by side, a scalar chain each (ChannelsN). Bit-identical: the
// same double operations in the same order, rounded to float where the original rounds (-0.0 included: the negation
// follows the multiply-add as there); flush to zero off as the original sets it.
// Stores made as the original's (its frame: LR save, the save slot of r31, a local of the copy so 0, the back chain,
// GetProcessingData's three words), so the guard compares everything. Declined: no buffer object, more than 6
// channels, a buffer that overlaps the filter's words or the frame, a range that is not one stretch of memory.
// Scratch not reproduced (hand.txt): r4-r12, f0, f10-f13. r1 ends as it came, r3 = 0.
namespace audio_filter {

[[gnu::always_inline]] inline uint32_t Load32(uint8_t* base, uint32_t a) {
  uint32_t v;
  std::memcpy(&v, ::banjo::hot::Raw(base, a), 4);
  return __builtin_bswap32(v);
}
[[gnu::always_inline]] inline float LoadF(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  v = __builtin_bswap32(v);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
[[gnu::always_inline]] inline void StoreF(uint8_t* p, float f) {
  uint32_t v;
  std::memcpy(&v, &f, 4);
  v = __builtin_bswap32(v);
  std::memcpy(p, &v, 4);
}
[[gnu::always_inline]] inline void Store32(uint8_t* p, uint32_t v) {
  v = __builtin_bswap32(v);
  std::memcpy(p, &v, 4);
}
[[gnu::always_inline]] inline bool Overlap(uint32_t a, uint32_t n, uint32_t b, uint32_t m) {
  return uint64_t(a) < uint64_t(b) + m && uint64_t(b) < uint64_t(a) + n;
}

template <int kMode, uint32_t N>
[[gnu::always_inline]] inline void ChannelsN(uint8_t* samples, float q, float freq, float* low, float* band) {
  float l[N], b[N];
  for (uint32_t c = 0; c < N; ++c) {
    l[c] = low[c];
    b[c] = band[c];
  }
  // banjo-nx (2026-10-23, batch 6): high = -h with h = float(fma(q, band, -d)) (rounding is symmetric: float(-x) is
  // -float(x)), and freq * high = (-freq) * h exactly, so the band update takes h and -freq: the negation leaves the
  // chain each sample waits on (one instruction of its latency). The same values, -0.0 included; a NaN's sign is the
  // only bit that can differ, as with the reverb (real audio has none).
  const double dq = double(q), dfreq = double(freq), ndfreq = -dfreq;
  for (uint32_t i = 0; i < 256; ++i) {
    for (uint32_t c = 0; c < N; ++c) {
      uint8_t* at = samples + 1024u * c + 4u * i;
      const double d = double(LoadF(at) - l[c]);                        // fsubs
      const float h = float(std::fma(dq, double(b[c]), -d));            // fnmsubs before its negation
      b[c] = float(std::fma(ndfreq, double(h), double(b[c])));          // fmadds: freq * high + band
      l[c] = float(std::fma(dfreq, double(b[c]), double(l[c])));        // fmadds
      StoreF(at, kMode > 0 ? -h : kMode < 0 ? l[c] : b[c]);
    }
  }
  for (uint32_t c = 0; c < N; ++c) {
    low[c] = l[c];
    band[c] = b[c];
  }
}

template <int kMode>
[[gnu::always_inline]] inline void Channels(uint8_t* samples, uint32_t nch, float q, float freq, float* low,
                                            float* band) {
  switch (nch) {
  case 1: ChannelsN<kMode, 1>(samples, q, freq, low, band); break;
  case 2: ChannelsN<kMode, 2>(samples, q, freq, low, band); break;
  case 3: ChannelsN<kMode, 3>(samples, q, freq, low, band); break;
  case 4: ChannelsN<kMode, 4>(samples, q, freq, low, band); break;
  case 5: ChannelsN<kMode, 5>(samples, q, freq, low, band); break;
  default: ChannelsN<kMode, 6>(samples, q, freq, low, band); break;
  }
}

template <class S>
[[gnu::always_inline]] inline bool Run(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!On(kAudioFilter)) [[unlikely]] return false;
  const uint32_t self = ctx.r3.u32, fb = ctx.r4.u32, sp = ctx.r1.u32, frame = sp - 112u;
  if (fb == 0 || self > 0xFFFFE000u || fb > 0xFFFFE000u || sp > 0xFFFFE000u || sp < 0x1000u) [[unlikely]]
    return false;
  // GetProcessingData's three words (+12, +16, +20 of the buffer object).
  const uint32_t w0 = Load32(base, fb + 12), w1 = Load32(base, fb + 16), samples = Load32(base, fb + 20);
  const uint32_t nch = (w0 >> 16) & 0xFFu;  // byte +81 of the frame: the word's second byte
  if (nch > 6) [[unlikely]] return false;
  if (::banjo::hot::PhysOff(self + 20) != ::banjo::hot::PhysOff(self + 83) ||
      ::banjo::hot::PhysOff(frame) != ::banjo::hot::PhysOff(sp - 1))
    [[unlikely]] return false;
  const uint32_t bytes = 1024u * nch;
  if (nch) {
    if (samples > 0xFFFFFFFFu - bytes || ::banjo::hot::PhysOff(samples) != ::banjo::hot::PhysOff(samples + bytes - 1) ||
        Overlap(samples, bytes, self + 20, 64) || Overlap(samples, bytes, frame, 112))
      [[unlikely]] return false;
  }
  s.Hand(kAudioFilter, nch);
  // The frame, as the original makes it (each store unless the build elides it).
#if !defined(REX_LR_MOVES_ELIDE)
  if (!s.Note(sp - 8, 4)) [[unlikely]] return true;
  Store32(::banjo::hot::Raw(base, sp - 8), uint32_t(ctx.lr));
#endif
#if !defined(REX_NV_SAVES_ELIDE)
  if (!s.Note(sp - 16, 8)) [[unlikely]] return true;
  std::memset(::banjo::hot::Raw(base, sp - 16), 0, 8);  // r31 is a local of the copy: its save writes 0
#endif
#if !defined(REX_BACK_CHAIN_ELIDE)
  if (!s.Note(frame, 4)) [[unlikely]] return true;
  Store32(::banjo::hot::Raw(base, frame), sp);
#endif
  if (!s.Note(frame + 80, 12)) [[unlikely]] return true;
  ::banjo::hot::Card(base, frame + 80, 12);
  uint8_t* const out = ::banjo::hot::Raw(base, frame + 80);
  Store32(out, w0);
  Store32(out + 4, w1);
  Store32(out + 8, samples);
  uint8_t* const f = ::banjo::hot::Raw(base, self);
  float low[6], band[6];
  if (f[24] != 0) {
    ctx.fpscr.disableFlushMode();
    if (!s.Note(self + 32, 48)) [[unlikely]] return true;
    ::banjo::hot::Card(base, self + 32, 48);
    std::memset(f + 32, 0, 48);  // the constant at 0x820062F8: 0.0f
  }
  if (nch) {
    ctx.fpscr.disableFlushMode();
    for (uint32_t c = 0; c < nch; ++c) {
      low[c] = LoadF(f + 32 + 4 * c);
      band[c] = LoadF(f + 56 + 4 * c);
    }
    const float q = LoadF(f + 28), freq = LoadF(f + 80);
    const int32_t mode = int32_t(Load32(base, self + 20));
    if (!s.Note(samples, bytes) || !s.Note(self + 32, 4 * nch) || !s.Note(self + 56, 4 * nch)) [[unlikely]]
      return true;
    ::banjo::hot::Card(base, samples, bytes);
    ::banjo::hot::Card(base, self + 32, 4 * nch);
    ::banjo::hot::Card(base, self + 56, 4 * nch);
    uint8_t* const p = ::banjo::hot::Raw(base, samples);
    if (mode > 0)
      Channels<1>(p, nch, q, freq, low, band);
    else if (mode < 0)
      Channels<-1>(p, nch, q, freq, low, band);
    else
      Channels<0>(p, nch, q, freq, low, band);
    for (uint32_t c = 0; c < nch; ++c) {
      StoreF(f + 32 + 4 * c, low[c]);
      StoreF(f + 56 + 4 * c, band[c]);
    }
  }
  ctx.r3.u64 = 0;
  return true;
}

}  // namespace audio_filter

namespace n_82B6B258 {
template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return audio_filter::Run(ctx, base, s);
}
}  // namespace n_82B6B258

// XAudio's ramped channel mix, rex_XAudioMixFrameBufferChannel (banjo-nx 2026-10-23, batch 6, the user's request: the
// audio mixer further): the loop of XAUDIO::CRouterEffect::Process, a sixth of the audio thread in the console's city
// profile. r3 = the destination channel, r4 = the source channel (256 big-endian floats each), r5 = the gain at the
// first sample (a vector, address rounded down to 16), r6 = the gain step per sample (likewise). The original (hand
// written VMX): saves r31 at -8(r1) (a local of the copy, so 0: the save writes 0 unless the build elides it), sets the
// flush-to-zero mode, makes the four lanes' gains g0 = gain, g1 = g0 + s, g2 = g0 + 2s, g3 = g0 + (2s + s) and the step
// 4s = 2s + 2s, then for 16 blocks of 64 bytes loads the source's four vectors and the destination's four, computes
// dst_k = src_k * g_k + dst_k (vmaddfp as the generated code makes it: a multiply, then an add) and stores the four, and
// adds 4s to each gain. Here: the same operations on the same values, a block at a time in the same order (all of a
// block's loads before its stores: the same result where the channels overlap), each guest float byte-reversed once,
// the destination's store cards marked once for its 1024 bytes instead of at each vector store. Declined (the copy
// runs): a channel not 16-byte aligned, a range that is not one stretch of memory.
// Scratch not reproduced (hand.txt): r3-r10, v0-v13. r1 ends as it came.
namespace n_rex_XAudioMixFrameBufferChannel {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!On(kXAudioMix)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32, gain = ctx.r5.u32 & ~15u, step = ctx.r6.u32 & ~15u;
  if (((dst | src) & 15u) || !detail::Straight(dst, 1024) || !detail::Straight(src, 1024) ||
      !detail::Straight(gain, 16) || !detail::Straight(step, 16)) [[unlikely]]
    return false;
  s.Hand(kXAudioMix, 256);
#if !defined(REX_NV_SAVES_ELIDE)
  {
    const uint32_t slot = ctx.r1.u32 - 8u;  // std r31,-8(r1): the local r31, zero
    if (!s.Note(slot, 8)) [[unlikely]] return true;
    std::memset(::banjo::hot::Raw(base, slot), 0, 8);
  }
#endif
  ctx.fpscr.enableFlushMode();
  if (!s.Note(dst, 1024)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 1024);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  const uint8_t* p = ::banjo::hot::Raw(base, src);
#if defined(__aarch64__)
  auto load = [](const uint8_t* at) { return vreinterpretq_f32_u8(vrev32q_u8(vld1q_u8(at))); };
  auto store = [](uint8_t* at, float32x4_t v) { vst1q_u8(at, vrev32q_u8(vreinterpretq_u8_f32(v))); };
  const float32x4_t sv = load(::banjo::hot::Raw(base, step));
  const float32x4_t s2 = vaddq_f32(sv, sv);
  float32x4_t g0 = load(::banjo::hot::Raw(base, gain));
  float32x4_t g1 = vaddq_f32(g0, sv);
  float32x4_t g2 = vaddq_f32(g0, s2);
  float32x4_t g3 = vaddq_f32(g0, vaddq_f32(s2, sv));
  const float32x4_t s4 = vaddq_f32(s2, s2);
  for (uint32_t block = 0; block < 16; ++block, d += 64, p += 64) {
    const float32x4_t a0 = load(p), a1 = load(p + 16), a2 = load(p + 32), a3 = load(p + 48);
    const float32x4_t b0 = load(d), b1 = load(d + 16), b2 = load(d + 32), b3 = load(d + 48);
    store(d, vaddq_f32(vmulq_f32(a0, g0), b0));
    store(d + 16, vaddq_f32(vmulq_f32(a1, g1), b1));
    store(d + 32, vaddq_f32(vmulq_f32(a2, g2), b2));
    store(d + 48, vaddq_f32(vmulq_f32(a3, g3), b3));
    g0 = vaddq_f32(g0, s4);
    g1 = vaddq_f32(g1, s4);
    g2 = vaddq_f32(g2, s4);
    g3 = vaddq_f32(g3, s4);
  }
#else
  // PC: lane by lane, each a float multiply then an add (the build does not contract them).
  auto load = [](const uint8_t* at, float out[4]) {
    for (uint32_t k = 0; k < 4; ++k)
      out[k] = audio_filter::LoadF(at + 4 * k);
  };
  float sv[4], g[4][4], s4[4];
  load(::banjo::hot::Raw(base, step), sv);
  load(::banjo::hot::Raw(base, gain), g[0]);
  for (uint32_t k = 0; k < 4; ++k) {
    const float s2 = sv[k] + sv[k];
    g[1][k] = g[0][k] + sv[k];
    g[2][k] = g[0][k] + s2;
    g[3][k] = g[0][k] + (s2 + sv[k]);
    s4[k] = s2 + s2;
  }
  for (uint32_t block = 0; block < 16; ++block, d += 64, p += 64) {
    float a[4][4], b[4][4];
    for (uint32_t v = 0; v < 4; ++v) {
      load(p + 16 * v, a[v]);
      load(d + 16 * v, b[v]);
    }
    for (uint32_t v = 0; v < 4; ++v) {
      for (uint32_t k = 0; k < 4; ++k) {
        const float product = a[v][k] * g[v][k];
        audio_filter::StoreF(d + 16 * v + 4 * k, product + b[v][k]);
        g[v][k] = g[v][k] + s4[k];
      }
    }
  }
#endif
  return true;
}

}  // namespace n_rex_XAudioMixFrameBufferChannel

// ---------------------------------------------------------------------------------------------------------------------
// katamari-nx (2026-10-30): Beautiful Katamari's hand-written natives. Chosen from the PC sampling profile of title,
// menus, intro and the first gameplay (docs/katamari-notes.md, "Hot functions"): the hottest guest functions that are
// pure data transforms, whose result can be written exactly from what the generated code does. The tree walkers of
// ranks 3-5 (virtual calls, a scene graph) are left to the localized copies (localized.txt).
//
// Every one reproduces EVERY register its original writes (hand.txt: exact= names all of them), so the guard compares
// everything, and calls the same flush-mode switch at the same point. They decline (the generated copy runs) where
// the generated code's result is not a plain copy: a single-precision signalling NaN (the generated lfs / stfs pair
// widens to double and back, which quiets it on both hosts), a destination ahead of an overlapping source, a range
// that is not one stretch of memory.

namespace katamari {

// The in-run A/B's target 69 (renut_engine/gpu/banjo_renut_tuning.cpp) sends Katamari's natives back to the generated
// copies for its off half.
[[gnu::always_inline]] inline bool KOn(uint32_t which) {
  return On(which) && ::banjo::renut_gpu::g_ab_off.load(std::memory_order_relaxed) != 69;
}

// A guest float word (big-endian in memory) that the generated code's float -> double -> float would change: a
// signalling NaN (exponent all ones, quiet bit clear, mantissa not zero).
[[gnu::always_inline]] inline bool Snan(uint32_t w) { return (w & 0x7FC00000u) == 0x7F800000u && (w & 0x003FFFFFu); }

[[gnu::always_inline]] inline uint32_t LoadW(const uint8_t* p) {
  uint32_t w;
  std::memcpy(&w, p, 4);
  return __builtin_bswap32(w);
}

// lfs as the processed generated code makes it (codegen fp_copy_pairs: rex::ppc::SingleToDoublePure): the value
// double(float) gives with the flush mode off, without reading or touching the mode.
[[gnu::always_inline]] inline double WidenF(uint32_t w) { return ::rex::ppc::SingleToDoublePure(w); }

// A 16-byte guest vector at any address into the register layout of the generated code (the lvlx + lvrx + vor of an
// unaligned load, or one lvx: u8[15 - i] = byte i), and back (stvlx + stvrx: byte i = u8[15 - i]).
[[gnu::always_inline]] inline simde__m128i Reverse16(simde__m128i v) {
  return simde_mm_shuffle_epi8(v, simde_mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));
}
[[gnu::always_inline]] inline simde__m128i LoadV(const uint8_t* p) {
  return Reverse16(simde_mm_loadu_si128(reinterpret_cast<const simde__m128i*>(p)));
}
[[gnu::always_inline]] inline void StoreV(uint8_t* p, simde__m128i v) {
  simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(p), Reverse16(v));
}
[[gnu::always_inline]] inline simde__m128i Dp(simde__m128i a, simde__m128i b) {
  return simde_mm_castps_si128(simde_mm_dp_ps(simde_mm_castsi128_ps(a), simde_mm_castsi128_ps(b), 0xFF));
}
[[gnu::always_inline]] inline void PutV(PPCVRegister& r, simde__m128i v) {
  simde_mm_store_si128(reinterpret_cast<simde__m128i*>(r.u8), v);
}

// The 4x4 multiply of sub_827598C0 / sub_82759780 (the same 76 instructions, the first operand's address in r3 or
// r5): rows a0..a3 of the first matrix (transposed in registers by vmrghw / vmrglw), rows b0..b3 of the second,
// sixteen vmsum4fp128 (simde_mm_dp_ps, mask 0xFF, as the generated code computes them) and the result's rows stored to
// r3. All eight loads come before the first store, as in the original (in-place calls are fine). Every register the
// original writes back is written with the value it leaves: r8-r10 and v0-v13 (r11 and v29-v31 are locals of the
// generated function since non_argument_as_local / non_volatile_as_local, 2026-10-30 evening).
template <class S>
[[gnu::always_inline]] inline bool MatMul(PPCContext& __restrict ctx, uint8_t* base, S& s, uint32_t a_va,
                                          uint32_t which) {
  if (!KOn(which)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, b_va = ctx.r4.u32;
  if (!detail::Straight(dst, 64) || !detail::Straight(a_va, 64) || !detail::Straight(b_va, 64)) [[unlikely]]
    return false;
  s.Hand(which, 64);
  const uint8_t* pa = ::banjo::hot::Raw(base, a_va);
  const uint8_t* pb = ::banjo::hot::Raw(base, b_va);
  // The generated statements, in their order, on locals (vmrghw vD,vA,vB = unpackhi_epi32(vB, vA) in this layout).
  simde__m128i v12 = LoadV(pa), v11 = LoadV(pa + 16), v10 = LoadV(pa + 32), v9 = LoadV(pa + 48);
  simde__m128i v0 = LoadV(pb), v13 = LoadV(pb + 16);
  simde__m128i v8 = simde_mm_unpackhi_epi32(v10, v12);
  simde__m128i v6 = simde_mm_unpacklo_epi32(v10, v12);
  simde__m128i v7 = simde_mm_unpackhi_epi32(v9, v11);
  simde__m128i v5 = simde_mm_unpacklo_epi32(v9, v11);
  v12 = LoadV(pb + 32);
  v11 = LoadV(pb + 48);
  v10 = simde_mm_unpackhi_epi32(v7, v8);
  v9 = simde_mm_unpackhi_epi32(v5, v6);
  v8 = simde_mm_unpacklo_epi32(v7, v8);
  v7 = simde_mm_unpacklo_epi32(v5, v6);
  ctx.fpscr.enableFlushMode();
  v6 = Dp(v0, v10);
  simde__m128i v3 = Dp(v0, v9);
  simde__m128i v1 = Dp(v0, v8);
  v0 = Dp(v0, v7);
  v5 = Dp(v13, v10);
  simde__m128i v2 = Dp(v13, v9);
  const simde__m128i v31 = Dp(v13, v8);
  v13 = Dp(v13, v7);
  simde__m128i v4 = Dp(v12, v10);
  const simde__m128i v30 = Dp(v12, v7);
  const simde__m128i v29 = Dp(v12, v8);
  v12 = Dp(v12, v9);
  v7 = Dp(v11, v7);
  v8 = Dp(v11, v8);
  v9 = Dp(v11, v9);
  v11 = Dp(v11, v10);
  v10 = simde_mm_unpackhi_epi32(v3, v6);
  v0 = simde_mm_unpackhi_epi32(v0, v1);
  v6 = simde_mm_unpackhi_epi32(v2, v5);
  v0 = simde_mm_unpackhi_epi32(v0, v10);
  v13 = simde_mm_unpackhi_epi32(v13, v31);
  v5 = simde_mm_unpackhi_epi32(v30, v29);
  v12 = simde_mm_unpackhi_epi32(v12, v4);
  v13 = simde_mm_unpackhi_epi32(v13, v6);
  v10 = simde_mm_unpackhi_epi32(v7, v8);
  v12 = simde_mm_unpackhi_epi32(v5, v12);
  v11 = simde_mm_unpackhi_epi32(v9, v11);
  v11 = simde_mm_unpackhi_epi32(v10, v11);
  if (!s.Note(dst, 64)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 64);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  StoreV(d, v0);
  StoreV(d + 16, v13);
  StoreV(d + 32, v12);
  StoreV(d + 48, v11);
  ctx.r8.s64 = 64;
  ctx.r9.s64 = 16;
  ctx.r10.s64 = 32;
  PutV(ctx.v0, v0);
  PutV(ctx.v1, v1);
  PutV(ctx.v2, v2);
  PutV(ctx.v3, v3);
  PutV(ctx.v4, v4);
  PutV(ctx.v5, v5);
  PutV(ctx.v6, v6);
  PutV(ctx.v7, v7);
  PutV(ctx.v8, v8);
  PutV(ctx.v9, v9);
  PutV(ctx.v10, v10);
  PutV(ctx.v11, v11);
  PutV(ctx.v12, v12);
  PutV(ctx.v13, v13);
  return true;
}

}  // namespace katamari

// sub_82759C98 - a 4x4 float matrix copy: r3 = destination, r4 = source; sixteen lfs f0 / stfs f0 pairs, word by
// word upward (the hottest guest function of the PC profile: 5.2 % of every busy sample, called for each matrix the
// game copies). Here: the 64 bytes as they are (a word copy upward is memmove's result for dst <= src). No register
// is written: with non_argument_as_local (2026-10-30 evening) f0 is a local of the generated function; the flush mode
// is not touched (the processed original's lfs /
// stfs pairs are mode-free since the post-codegen passes: the PC guard found the first version switching it). Declined: a destination ahead of an overlapping source (the
// original then copies words it already wrote), a signalling NaN among the words (the generated widening quiets it).
namespace n_82759C98 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!katamari::KOn(kMatCopy)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32;
  if (uint32_t(dst - src - 1u) < 63u) [[unlikely]] return false;  // dst > src, dst - src < 64
  if (!detail::Straight(dst, 64) || !detail::Straight(src, 64)) [[unlikely]] return false;
  const uint8_t* p = ::banjo::hot::Raw(base, src);
  uint32_t nan = 0;
  for (uint32_t k = 0; k < 16; ++k) nan |= uint32_t(katamari::Snan(katamari::LoadW(p + 4 * k)));
  if (nan) [[unlikely]] return false;
  s.Hand(kMatCopy, 64);
  if (!s.Note(dst, 64)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 64);
  detail::CopySmall(::banjo::hot::Raw(base, dst), p, 64);
  return true;
}

}  // namespace n_82759C98

// sub_827598C0 - 4x4 matrix multiply in place: r3 = r3 x r4 (first operand at r3). katamari::MatMul above.
namespace n_827598C0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return katamari::MatMul(ctx, base, s, ctx.r3.u32, kMatMul);
}

}  // namespace n_827598C0

// sub_82759780 - the same multiply with three operands: r3 = r5 x r4. katamari::MatMul above.
namespace n_82759780 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return katamari::MatMul(ctx, base, s, ctx.r5.u32, kMatMul);
}

}  // namespace n_82759780

// sub_82759D20 - a translation matrix: r3 = identity with the last row (x, y, z, 1) from the three floats at r4. The
// original loads 1.0 and 0.0 from the image's constants (0x82000B38 / 0x82000B34: read here too, not assumed) and
// stores through f13 / f0, so f13 = the one at its end (f0 and r11 are locals of the generated function since the
// register-diet codegen of 2026-10-30 evening).
// Declined: a signalling NaN among the five words, a destination that overlaps the three source floats (the original
// interleaves those loads with its stores).
namespace n_82759D20 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!katamari::KOn(kMatTranslation)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32;
  if (!detail::Straight(dst, 64) || !detail::Straight(src, 12)) [[unlikely]] return false;
  if (uint32_t(src - dst) < 64u || uint32_t(dst - src) < 12u) [[unlikely]] return false;
  if (uint32_t(0x82000B34u + 7u - dst) < 64u + 7u) [[unlikely]] return false;  // dst covers the two constants
  const uint32_t one = katamari::LoadW(::banjo::hot::Raw(base, 0x82000B38u));
  const uint32_t zero = katamari::LoadW(::banjo::hot::Raw(base, 0x82000B34u));
  const uint8_t* p = ::banjo::hot::Raw(base, src);
  const uint32_t x = katamari::LoadW(p), y = katamari::LoadW(p + 4), z = katamari::LoadW(p + 8);
  if (katamari::Snan(one) | katamari::Snan(zero) | katamari::Snan(x) | katamari::Snan(y) | katamari::Snan(z))
    [[unlikely]] return false;
  s.Hand(kMatTranslation, 64);
  // Each word goes out as float(double(float)) of its own bits: the same bits for every value but a signalling NaN.
  const uint32_t words[16] = {one, zero, zero, zero, zero, one, zero, zero, zero, zero, one, zero, x, y, z, one};
  if (!s.Note(dst, 64)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 64);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  for (uint32_t k = 0; k < 16; ++k) {
    const uint32_t be = __builtin_bswap32(words[k]);
    std::memcpy(d + 4 * k, &be, 4);
  }
  ctx.f13.f64 = katamari::WidenF(one);  // r11 and f0 are locals of the generated function (2026-10-30 evening)
  return true;
}

}  // namespace n_82759D20

// sub_827E7A40 - Beautiful Katamari's XDK C runtime memset: the same 40 instructions as banjo-nx's sub_82C116F0
// (compared word for word, 2026-10-30), so the same native (n_82C116F0 above) under the banjo_hand_memset key.
namespace n_827E7A40 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return n_82C116F0::Native(ctx, base, s);
}

}  // namespace n_827E7A40

// ---------------------------------------------------------------------------------------------------------------------
// katamari-nx (2026-10-30 evening): five more, chosen from the Ryujinx (ARM) profile of the package rc1
// (docs/katamari-notes.md, "Hot functions on ARM").

// sub_827E6F50 - Beautiful Katamari's XDK C runtime memcpy. Another XDK build than N&B's sub_82C111C0 (the block loops
// are scheduled differently), with the same contract, read from its 278 instructions: `std r3,-8(r1)` first and
// `ld r3,-8(r1)` at each of its four returns; a byte or word head up to 8-byte destination alignment, then 128-byte
// blocks, doublewords, words or bytes by the source's alignment, every unit's loads ahead of its stores and the units
// upward (for dst <= src the result is memmove's); sizes and addresses their 32-bit parts. So banjo-nx's native
// (n_82C111C0, sa2-nx's) under the banjo_hand_memcpy key: it declines a forward overlap, ranges that wrap and a
// destination over the save slot, writes the slot as the original does and leaves r3 as the original returns it.
namespace n_827E6F50 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  return n_82C111C0::Native(ctx, base, s);
}

}  // namespace n_827E6F50

// sub_827595D0 - the identity matrix: r3 = 16 words, 1.0 on the diagonal and 0.0 elsewhere, from the image's constants
// (0x82000B38 / 0x82000B34, the translation builder's). f13 = 1.0 at its end (f0 and r11 are locals of the generated
// function since 2026-10-30 evening); no flush-mode switch in the processed original. Declined: a destination over the two constants.
namespace n_827595D0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!katamari::KOn(kMatIdentity)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32;
  if (!detail::Straight(dst, 64)) [[unlikely]] return false;
  if (uint32_t(0x82000B34u + 7u - dst) < 64u + 7u) [[unlikely]] return false;  // dst covers the two constants
  const uint32_t one = katamari::LoadW(::banjo::hot::Raw(base, 0x82000B38u));
  const uint32_t zero = katamari::LoadW(::banjo::hot::Raw(base, 0x82000B34u));
  if (katamari::Snan(one) | katamari::Snan(zero)) [[unlikely]] return false;
  s.Hand(kMatIdentity, 64);
  if (!s.Note(dst, 64)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 64);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  const uint32_t be_one = __builtin_bswap32(one), be_zero = __builtin_bswap32(zero);
  for (uint32_t k = 0; k < 16; ++k) std::memcpy(d + 4 * k, (k % 5 == 0) ? &be_one : &be_zero, 4);
  ctx.f13.f64 = katamari::WidenF(one);  // r11 and f0 are locals of the generated function (2026-10-30 evening)
  return true;
}

}  // namespace n_827595D0

// sub_82759A00 - two 4x4 matrices equal within a tolerance: r3 = 1 when |b[k] - a[k]| < eps for all sixteen floats
// (a at r3, b at r4, eps the image constant 0x350637BD, folded in the processed original), else 0 at the first k that
// is not. Each step, as the original: fsubs (REX_FSUBS, the same expression), the single stored to the red-zone word
// at -16(r1), its sign bit cleared there, read back and compared. So the word at -16(r1) ends as the last |difference|
// (written once here; the guard compares memory), f13 as it widened, f12 as the last a[k] (from k = 1) and r3 = 0 / 1
// (f0 and r11 are locals of the generated function); the flush mode switched off first, as the original does. cr6 is a local of the
// generated function (not written back). Declined: a red-zone word inside either matrix (the original's stores there
// would change what it reads next).
namespace n_82759A00 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kMatCompare)) [[unlikely]] return false;
  const uint32_t a_va = ctx.r3.u32, b_va = ctx.r4.u32, slot = ctx.r1.u32 - 16u;
  if (!detail::Straight(a_va, 64) || !detail::Straight(b_va, 64) || !detail::Straight(slot, 4)) [[unlikely]]
    return false;
  if (uint32_t(slot + 3u - a_va) < 64u + 3u || uint32_t(slot + 3u - b_va) < 64u + 3u) [[unlikely]] return false;
  s.Hand(kMatCompare, 64);
  ctx.fpscr.disableFlushMode();
  const uint8_t* pa = ::banjo::hot::Raw(base, a_va);
  const uint8_t* pb = ::banjo::hot::Raw(base, b_va);
  const uint32_t eps_bits = 0x350637BDu;
  float eps_f;
  std::memcpy(&eps_f, &eps_bits, 4);
  const double eps = double(eps_f);  // f0 in the original
  uint32_t k = 0, masked = 0;
  double a_k = 0.0, abs_k = 0.0;
  bool all = true;
  for (;; ++k) {
    float fa, fb;
    const uint32_t wa = katamari::LoadW(pa + 4 * k), wb = katamari::LoadW(pb + 4 * k);
    std::memcpy(&fa, &wa, 4);
    std::memcpy(&fb, &wb, 4);
    a_k = double(fa);
    const double diff = REX_FSUBS(double(fb), a_k);
    const float single = float(diff);
    uint32_t bits;
    std::memcpy(&bits, &single, 4);
    masked = bits & 0x7FFFFFFFu;
    float abs_f;
    std::memcpy(&abs_f, &masked, 4);
    abs_k = double(abs_f);
    if (!(abs_k < eps)) {
      all = false;
      break;
    }
    if (k == 15) break;
  }
  if (!s.Note(slot, 4)) [[unlikely]] return true;
  const uint32_t be = __builtin_bswap32(masked);
  std::memcpy(::banjo::hot::Raw(base, slot), &be, 4);
  ctx.f13.f64 = abs_k;
  if (k != 0) ctx.f12.f64 = a_k;
  ctx.r3.u64 = all ? 1u : 0u;  // f0 and r11 are locals of the generated function (2026-10-30 evening)
  return true;
}

}  // namespace n_82759A00

// sub_827E73A8 - a C runtime copy of r5 bytes from r4 to r3 (r3 is not written): single bytes until the destination
// cursor is 4-aligned (ctr = r5 + 1 bounds them), then words (lwz, or four lbz assembled when the source is not
// aligned: the same bytes), then up to three bytes; every unit loaded before it is stored, upward. Declined: anything
// in the upper half of r5 (ctr is 64-bit), a destination ahead of an overlapping source, ranges that wrap. Scratch it
// leaves and this does not reproduce (the guard skips them): r0, r4-r10, ctr.
namespace n_827E73A8 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!katamari::KOn(kCopyWords)) [[unlikely]] return false;
  if (ctx.r5.u64 >> 32) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32, n = ctx.r5.u32;
  s.Hand(kCopyWords, n);
  if (n == 0) return true;
  if (uint32_t(dst - src - 1u) < n - 1u) [[unlikely]] return false;  // dst > src, dst - src < n
  if (!detail::Straight(dst, n) || !detail::Straight(src, n)) [[unlikely]] return false;
  if (!s.Note(dst, n)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, n);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  const uint8_t* p = ::banjo::hot::Raw(base, src);
  if (n <= 64)
    detail::CopySmall(d, p, n);
  else if (n <= 128)
    detail::CopyMedium(d, p, n);
  else
    std::memmove(d, p, n);  // dst < src overlapping (the forward copy is memmove's then) or apart
  return true;
}

}  // namespace n_827E73A8

// sub_8266AF60 - Jenkins' one-at-a-time hash of the NUL-terminated string whose address is the word at r3: for each byte
// h += c, h += h << 10, h ^= h >> 6; then h += h << 3, h ^= h >> 11, h += h << 15. In 64-bit registers as the original
// (the adds are 64-bit, every shift a 32-bit rlwinm), so r3 and r9 keep the carries above bit 31 the original keeps.
// The original leaves r3 (the hash), r8, r9 and r10 (leaf locals written back at its return; r8 untouched for the empty
// string); r11 is a local of the generated function. Declined: a string in the physical-address range (the generated
// code's loads there go through another view on the PC) or one longer than 64 KB (the walk stops there).
namespace n_8266AF60 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, [[maybe_unused]] S& s) {
  if (!katamari::KOn(kStringHash)) [[unlikely]] return false;
  const uint32_t at = ctx.r3.u32;
  if (!detail::Straight(at, 4)) [[unlikely]] return false;
  const uint32_t str = katamari::LoadW(::banjo::hot::Raw(base, at));
  if (str >= 0xDFFF0000u) [[unlikely]] return false;
  const uint8_t* p = ::banjo::hot::Raw(base, str);
  uint64_t r8 = ctx.r8.u64, r9 = 0, r10 = p[0];
  uint32_t k = 0;
  while (r10 != 0) {
    r9 = r10 + r9;
    ++k;
    r8 = uint32_t(uint32_t(r9) << 10);
    r9 = r8 + r9;
    if (k >= 0x10000u) [[unlikely]] return false;  // nothing written yet
    r10 = p[k];
    r8 = uint32_t(r9) >> 6;
    r9 = r8 ^ r9;
  }
  s.Hand(kStringHash, k);
  uint64_t r11 = uint32_t(uint32_t(r9) << 3);
  r11 = r9 + r11;
  r10 = uint32_t(r11) >> 11;
  r11 = r10 ^ r11;
  r10 = uint32_t(uint32_t(r11) << 15);
  ctx.r3.u64 = r10 + r11;
  ctx.r8.u64 = r8;
  ctx.r9.u64 = r9;
  ctx.r10.u64 = r10;
  return true;
}

}  // namespace n_8266AF60

// sub_8271FAD0 - a 4-float parameter set with change detection: element r4 (-1: nothing) of the table at
// [[r3 + 372] + 16], 72 bytes each. When the four floats at r5 differ from the element's first four (fcmpu: a NaN
// differs, -0.0 equals 0.0), or they are equal but the word at +64 is not 4, the element gets the four floats (each
// quieted as the processed original's lfs / stfs pairs give it: QuietSingleBits), +64 = 4 and the halfword at +68 one
// more; otherwise nothing is written. As the processed original: the flush mode switched off after the -1 test; r10,
// f12 and f13 left as it leaves them (f12 only from the second compare on); f0 and r11 are its locals. Declined:
// source and element overlapping, ranges that are not one stretch of memory.
namespace n_8271FAD0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kVec4Update)) [[unlikely]] return false;
  if (ctx.r4.s32 == -1) return true;
  const uint32_t holder = ctx.r3.u32 + 372u, src = ctx.r5.u32;
  if (!detail::Straight(holder, 4) || !detail::Straight(src, 16)) [[unlikely]] return false;
  const uint32_t table_ptr = katamari::LoadW(::banjo::hot::Raw(base, holder)) + 16u;
  if (!detail::Straight(table_ptr, 4)) [[unlikely]] return false;
  const uint64_t r10a = uint32_t(ctx.r4.u32 << 3);
  const uint64_t r10b = ctx.r4.u64 + r10a;
  const uint32_t offset = uint32_t(uint32_t(r10b) << 3);
  const uint32_t el = uint32_t(uint64_t(katamari::LoadW(::banjo::hot::Raw(base, table_ptr))) + offset);
  if (!detail::Straight(el, 70)) [[unlikely]] return false;
  if (uint32_t(src - el) < 70u || uint32_t(el - src) < 16u) [[unlikely]] return false;
  ctx.fpscr.disableFlushMode();
  s.Hand(kVec4Update, 16);
  const uint8_t* ps = ::banjo::hot::Raw(base, src);
  uint8_t* pe = ::banjo::hot::Raw(base, el);
  uint32_t ws[4], we[4];
  for (uint32_t k = 0; k < 4; ++k) {
    ws[k] = katamari::LoadW(ps + 4 * k);
    we[k] = katamari::LoadW(pe + 4 * k);
  }
  auto widen = [](uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return double(f);  // the original's double(REX_LOAD_D_F32(...)) with the flush mode off
  };
  bool equal = true;
  uint32_t k = 0;
  for (; k < 4; ++k) {
    ctx.f13.f64 = widen(we[k]);
    if (k) ctx.f12.f64 = widen(ws[k]);
    if (!(ctx.f13.f64 == (k ? ctx.f12.f64 : widen(ws[0])))) {
      equal = false;
      break;
    }
  }
  if (equal) {
    const uint32_t state = katamari::LoadW(pe + 64);
    if (state == 4u) {
      ctx.r10.u64 = state;
      return true;
    }
  }
  if (!s.Note(el, 16) || !s.Note(el + 64u, 6)) [[unlikely]] return true;
  ::banjo::hot::Card(base, el, 70);
  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t be = __builtin_bswap32(::rex::ppc::QuietSingleBits(ws[i]));
    std::memcpy(pe + 4 * i, &be, 4);
  }
  const uint32_t four = __builtin_bswap32(4u);
  std::memcpy(pe + 64, &four, 4);
  uint16_t count;
  std::memcpy(&count, pe + 68, 2);
  const uint64_t next = uint64_t(__builtin_bswap16(count)) + 1u;
  const uint16_t be16 = __builtin_bswap16(uint16_t(next));
  std::memcpy(pe + 68, &be16, 2);
  ctx.r10.u64 = next;
  return true;
}

}  // namespace n_8271FAD0

// ---------------------------------------------------------------------------------------------------------------------
// katamari-nx (2026-10-31): seven more natives, the user's request "hand optimise the top 7 functions of your choice".
// Chosen from the console profile of rc3 (builds/2026-10-31_0030_rc3_dlc_full, handheld clocks, two levels): the
// hottest guest functions that are pure data transforms with no guest call. (The hottest of all either call other
// guest functions, are already natives through their callees, are interlocked reference counts the guard cannot
// compare, or are the movie and audio codecs.) Unlike the first Katamari natives these follow banjo-nx's rule for
// scratch: the volatile registers the original leaves behind are not reproduced (hand.txt names the ones that are),
// so the work is the guest memory, the return value and the host flush mode, which the guard compares as before.

namespace katamari {

// [a, a + n) and [b, b + m) share a byte (n, m >= 1; neither wraps: callers test Straight first).
[[gnu::always_inline]] inline bool Overlap(uint32_t a, uint32_t n, uint32_t b, uint32_t m) {
  return uint32_t(a - b) < m || uint32_t(b - a) < n;
}

[[gnu::always_inline]] inline void StoreW(uint8_t* p, uint32_t w) {
  const uint32_t be = __builtin_bswap32(w);
  std::memcpy(p, &be, 4);
}

[[gnu::always_inline]] inline uint32_t FloatBits(double d) {
  const float f = float(d);  // d is already a single-precision value: exact in either flush mode
  uint32_t w;
  std::memcpy(&w, &f, 4);
  return w;
}

[[gnu::always_inline]] inline double Round1(double d) { return double(float(d)); }

// The host's flush-mode switch is an `msr fpcr` with no memory operand, so the compiler may move arithmetic on
// values it holds in registers across it. Each native puts this barrier right after a switch and reads the inputs
// of the arithmetic that must follow the switch from memory after it (and writes what must precede a switch to
// memory before it).
[[gnu::always_inline]] inline void Fence() { __asm__ __volatile__("" ::: "memory"); }

// For a value that lives only in registers (a local whose address the compiler sees does not escape), the barrier
// above does not order it: this ties the value to the point where it stands, in order with the switch.
template <class T>
[[gnu::always_inline]] inline void Pin(T& x) {
#if defined(__aarch64__)
  __asm__ __volatile__("" : "+w"(x));
#elif defined(__x86_64__) || defined(_M_X64)
  __asm__ __volatile__("" : "+x"(x));
#endif
}
[[gnu::always_inline]] inline void PinW(uint32_t& x) { __asm__ __volatile__("" : "+r"(x)); }

}  // namespace katamari

// sub_827596B0 - 4x4 matrix product r3 = r4 x r5: the rows of r4 dotted with the columns of r5 (the second operand
// transposed in registers by vmrghw / vmrglw, sixteen vmsum4fp128 as simde_mm_dp_ps). The original's statements in
// their order, on locals; all eight loads come before the first store (in place is fine), the addresses rounded down
// to 16 bytes as lvx128 / stvx128 do. Scratch not reproduced (hand.txt): r9, r10, v0-v13. The flush mode is switched
// on where the original switches it.
namespace n_827596B0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kMatProduct)) [[unlikely]] return false;
  const uint32_t dst = ctx.r3.u32 & ~15u, a = ctx.r4.u32 & ~15u, b = ctx.r5.u32 & ~15u;
  if (!detail::Straight(dst, 64) || !detail::Straight(a, 64) || !detail::Straight(b, 64)) [[unlikely]] return false;
  s.Hand(kMatProduct, 64);
  using katamari::Dp;
  using katamari::LoadV;
  const uint8_t* pa = ::banjo::hot::Raw(base, a);
  const uint8_t* pb = ::banjo::hot::Raw(base, b);
  // The original switches the mode before its first vmsum4fp128; the loads and merges do not depend on it, so the
  // switch comes first here and everything is read after it.
  ctx.fpscr.enableFlushMode();
  katamari::Fence();
  simde__m128i v11 = LoadV(pb), v0 = LoadV(pa), v10 = LoadV(pb + 16), v9 = LoadV(pb + 32), v8 = LoadV(pb + 48);
  simde__m128i v5 = simde_mm_unpacklo_epi32(v9, v11);
  simde__m128i v7 = simde_mm_unpackhi_epi32(v9, v11);
  simde__m128i v13 = LoadV(pa + 16);
  simde__m128i v6 = simde_mm_unpackhi_epi32(v8, v10);
  simde__m128i v12 = LoadV(pa + 32);
  simde__m128i v4 = simde_mm_unpacklo_epi32(v8, v10);
  v11 = LoadV(pa + 48);
  v10 = simde_mm_unpackhi_epi32(v6, v7);
  v9 = simde_mm_unpacklo_epi32(v6, v7);
  v8 = simde_mm_unpackhi_epi32(v4, v5);
  v7 = simde_mm_unpacklo_epi32(v4, v5);
  v6 = Dp(v0, v10);
  v5 = Dp(v0, v9);
  v4 = Dp(v0, v8);
  v0 = Dp(v0, v7);
  simde__m128i v3 = Dp(v13, v9);
  const simde__m128i v2 = Dp(v13, v7);
  const simde__m128i v1 = Dp(v13, v8);
  v13 = Dp(v13, v10);
  v6 = simde_mm_unpackhi_epi32(v4, v6);
  v4 = Dp(v12, v9);
  v0 = simde_mm_unpackhi_epi32(v0, v5);
  v9 = Dp(v11, v9);
  v5 = simde_mm_unpackhi_epi32(v2, v3);
  v3 = Dp(v12, v8);
  v8 = Dp(v11, v8);
  v0 = simde_mm_unpackhi_epi32(v0, v6);
  v6 = Dp(v12, v7);
  v12 = Dp(v12, v10);
  v13 = simde_mm_unpackhi_epi32(v1, v13);
  v7 = Dp(v11, v7);
  v11 = Dp(v11, v10);
  const simde__m128i row0 = v0;
  const simde__m128i row1 = simde_mm_unpackhi_epi32(v5, v13);
  v10 = simde_mm_unpackhi_epi32(v6, v4);
  v13 = simde_mm_unpackhi_epi32(v3, v12);
  v12 = simde_mm_unpackhi_epi32(v7, v9);
  v0 = simde_mm_unpackhi_epi32(v8, v11);
  const simde__m128i row2 = simde_mm_unpackhi_epi32(v10, v13);
  const simde__m128i row3 = simde_mm_unpackhi_epi32(v12, v0);
  if (!s.Note(dst, 64)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 64);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  katamari::StoreV(d, row0);
  katamari::StoreV(d + 16, row1);
  katamari::StoreV(d + 32, row2);
  katamari::StoreV(d + 48, row3);
  return true;
}

}  // namespace n_827596B0

// sub_8270FE40 - four 16-byte vectors (r4, r5, r6, r7) into the object at r3 + 4168 ... 4231, the single in f1 at
// r3 + 4232, and bit 0 set in the word at r3 + 3604. The original: sixteen lfs / stfs pairs (a signalling NaN comes
// out quiet: rex::ppc::QuietSingleBits, as the processed generated code does), the flag word read, the flush mode off,
// f1 stored, the flag written back. Here the sixteen words, the single and the flag go in three writes. Declined: a
// source that overlaps either destination (the original's word-by-word order would then show). No register written
// (f0 and r11 are locals of the generated function).
namespace n_8270FE40 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kVecSet)) [[unlikely]] return false;
  const uint32_t obj = ctx.r3.u32, dst = obj + 4168u, flag = obj + 3604u;
  const uint32_t src[4] = {ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32};
  if (!detail::Straight(dst, 68) || !detail::Straight(flag, 4)) [[unlikely]] return false;
  for (uint32_t k = 0; k < 4; ++k) {
    if (!detail::Straight(src[k], 16) || katamari::Overlap(src[k], 16, dst, 68) ||
        katamari::Overlap(src[k], 16, flag, 4)) [[unlikely]]
      return false;
  }
  s.Hand(kVecSet, 72);
  uint32_t w[17];
  for (uint32_t k = 0; k < 4; ++k) {
    const uint8_t* p = ::banjo::hot::Raw(base, src[k]);
    for (uint32_t j = 0; j < 4; ++j) w[4 * k + j] = ::rex::ppc::QuietSingleBits(katamari::LoadW(p + 4 * j));
  }
  const uint32_t flag_word = katamari::LoadW(::banjo::hot::Raw(base, flag)) | 1u;
  ctx.fpscr.disableFlushMode();
  katamari::Fence();
  w[16] = katamari::FloatBits(ctx.f1.f64);  // read after the switch: the original's stfs runs with the mode off
  if (!s.Note(dst, 68) || !s.Note(flag, 4)) [[unlikely]] return true;
  ::banjo::hot::Card(base, dst, 68);
  ::banjo::hot::Card(base, flag, 4);
  uint8_t* d = ::banjo::hot::Raw(base, dst);
  for (uint32_t k = 0; k < 17; ++k) katamari::StoreW(d + 4 * k, w[k]);
  katamari::StoreW(::banjo::hot::Raw(base, flag), flag_word);
  return true;
}

}  // namespace n_8270FE40

// sub_82316A78 - the worker threads' sphere test against a box (10 % of each of the four job threads on the console):
// r3 = 1 when the length of the point at r4 is below 2/3 of the length of the box's diagonal (r3's box: the word at
// r3 + 4 is its offset from r3; corners at +0 and +16), else 0. The original goes back and forth between the scalar
// unit (the point's length, the diagonal's components, flush mode off) and the vector unit (the diagonal's length by
// vmsum3fp128 and a Newton step of vrsqrtefp, flush mode on): four or five writes of the host's FPCR a call. Here no
// mode switch in the middle: the scalar steps through the rex::ppc *AnyMode helpers (the flush-off result in either
// mode) and the vector steps in whatever mode the host is in, which gives the original's flush-on results exactly when
// no denormal input or result can occur; where one could (a component of the diagonal below 2^-63 that is not zero, a
// squared length below 2^-124 or above 2^120, a point length below 2^-126) the call is declined before anything is
// written. The mode ends on, as in the original, with at most one switch. The stack temporary at r1 - 16 (the three
// components, then the vector over them) ends as the vector, written once; the stack pointer must be 16-aligned (the
// vector store rounds down), else declined. Scratch not reproduced (hand.txt): f9-f13, v0, v8-v13.
namespace n_82316A78 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kSphereBox)) [[unlikely]] return false;
  const uint32_t obj = ctx.r3.u32, pt = ctx.r4.u32, sp = ctx.r1.u32;
  if (sp & 15u) [[unlikely]] return false;
  const uint32_t tmp = sp - 16u;
  if (!detail::Straight(obj + 4u, 4) || !detail::Straight(pt, 12) || !detail::Straight(tmp, 16)) [[unlikely]]
    return false;
  const uint32_t box = obj + katamari::LoadW(::banjo::hot::Raw(base, obj + 4u));
  if (!detail::Straight(box, 28)) [[unlikely]] return false;
  const uint8_t* pp = ::banjo::hot::Raw(base, pt);
  const uint8_t* pb = ::banjo::hot::Raw(base, box);
  using ::rex::ppc::SingleToDoublePure;
  // The point's length (fmuls / fmadds / fmadds / fsqrts with the flush mode off).
  const double y = ::rex::ppc::SingleToDoubleAnyMode(katamari::LoadW(pp + 4));
  const double yy = ::rex::ppc::MulSingleAnyMode(y, y);
  const double x = ::rex::ppc::SingleToDoubleAnyMode(katamari::LoadW(pp));
  const double z = SingleToDoublePure(katamari::LoadW(pp + 8));
  const double s1 = std::fma(x, x, yy);
  if (s1 != 0.0 && std::fabs(s1) < 0x1p-126) [[unlikely]] return false;
  const double xy = katamari::Round1(s1);
  const double s2 = std::fma(z, z, xy);
  if (s2 != 0.0 && std::fabs(s2) < 0x1p-126) [[unlikely]] return false;
  const double dist = REX_FSQRTS(katamari::Round1(s2));
  // The diagonal (fsubs with the flush mode off).
  const double d0 = ::rex::ppc::SubSingleAnyMode(SingleToDoublePure(katamari::LoadW(pb + 16)),
                                                 SingleToDoublePure(katamari::LoadW(pb)));
  const double d1 = ::rex::ppc::SubSingleAnyMode(SingleToDoublePure(katamari::LoadW(pb + 20)),
                                                 SingleToDoublePure(katamari::LoadW(pb + 4)));
  const double d2 = ::rex::ppc::SubSingleAnyMode(SingleToDoublePure(katamari::LoadW(pb + 24)),
                                                 SingleToDoublePure(katamari::LoadW(pb + 8)));
  // The vector the original loads back from the stack: the three components and the fourth word as it was.
  alignas(16) uint8_t vbuf[16];
  katamari::StoreW(vbuf, katamari::FloatBits(d0));
  katamari::StoreW(vbuf + 4, katamari::FloatBits(d1));
  katamari::StoreW(vbuf + 8, katamari::FloatBits(d2));
  std::memcpy(vbuf + 12, ::banjo::hot::Raw(base, tmp + 12u), 4);
  const simde__m128 v13 = simde_mm_castsi128_ps(katamari::LoadV(vbuf));
  const simde__m128 v0 = simde_mm_dp_ps(v13, v13, 0xEF);
  if (!ctx.fpscr.flushModeIsOn()) {
    const double d[3] = {d0, d1, d2};
    for (double c : d)
      if (c != 0.0 && std::fabs(c) < 0x1p-63) [[unlikely]] return false;
    float l;
    simde_mm_store_ss(&l, v0);
    if ((l > 0.0f && l < 0x1p-124f) || l > 0x1p120f) [[unlikely]] return false;
  }
  const simde__m128 half = simde_mm_mul_ps(simde_mm_cvtepi32_ps(simde_mm_set1_epi32(1)),
                                           simde_mm_castsi128_ps(simde_mm_set1_epi32(int(0x3F000000))));
  const simde__m128 est = simde_mm_div_ps(simde_mm_set1_ps(1), simde_mm_sqrt_ps(v0));
  const simde__m128 v9 = simde_mm_mul_ps(v0, half);
  const simde__m128 v11 = simde_mm_cmpeq_ps(v0, simde_mm_castsi128_ps(simde_mm_set1_epi32(0)));
  const simde__m128 v8 = simde_mm_mul_ps(est, est);
  const simde__m128 v12 = simde_mm_xor_ps(simde_mm_sub_ps(simde_mm_mul_ps(v9, v8), half),
                                          simde_mm_castsi128_ps(simde_mm_set1_epi32(int(0x80000000))));
  const simde__m128 step = simde_mm_add_ps(simde_mm_mul_ps(est, v12), est);
  const simde__m128 len = simde_mm_mul_ps(v0, step);
  const simde__m128i out = REX_VSEL(simde_mm_castps_si128(v11), simde_mm_castps_si128(len), simde_mm_castps_si128(v0));
  alignas(16) uint8_t obuf[16];
  katamari::StoreV(obuf, out);
  const double f12 = SingleToDoublePure(katamari::LoadW(obuf));
  const double third2 = SingleToDoublePure(REX_RO_CFG_U32(0x820563DC, 0x3F2AAAABu, REX_LOAD_U32(0x820563DCu)));
  const double limit = ::rex::ppc::MulSingleAnyMode(f12, third2);
  PPCCRRegister cr6{};
  ::rex::ppc::CompareAnyMode(cr6, dist, limit);
  s.Hand(kSphereBox, 16);
  ctx.r3.u64 = cr6.lt ? 1u : 0u;
  if (!s.Note(tmp, 16)) [[unlikely]] return true;
  ::banjo::hot::Card(base, tmp, 16);
  std::memcpy(::banjo::hot::Raw(base, tmp), obuf, 16);
  katamari::Fence();  // everything above in the entry mode
  ctx.fpscr.enableFlushMode();
  return true;
}

}  // namespace n_82316A78

// sub_8275A828 - r6 three-vectors at r4 turned by the 3x3 part of the 4x4 matrix at r3 into r5 (twelve bytes each;
// four at a time, then one at a time, the same arithmetic). The original reads the nine matrix elements again for
// every vector (a store could change them); here they are read once, after checking that the output does not overlap
// the matrix (else declined), and each vector is read, turned and written in the original's order, so outputs that
// overlap the inputs give what the original gives. Arithmetic as generated, with the flush mode off as the original
// switches it before its loops (when there is a vector at all): out.x = R(fma(m0, x, R(fma(m32, z, R(m16 y))))),
// out.y the same with m20, m36, m4, out.z = R(fma(y, m24, R(fma(m8, x, R(z m40))))), R the rounding to single.
// A negative or very large count is declined (the original's unsigned tail count would run on). Scratch not
// reproduced (hand.txt): r7-r10, f9-f13.
namespace n_8275A828 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kRotate)) [[unlikely]] return false;
  const uint32_t mtx = ctx.r3.u32, in = ctx.r4.u32, out = ctx.r5.u32;
  const int32_t n = ctx.r6.s32;
  if (n < 0 || n > 65536) [[unlikely]] return false;
  if (n == 0) return true;
  const uint32_t bytes = 12u * uint32_t(n);
  if (!detail::Straight(mtx, 44) || !detail::Straight(in, bytes) || !detail::Straight(out, bytes) ||
      katamari::Overlap(out, bytes, mtx, 44)) [[unlikely]]
    return false;
  s.Hand(kRotate, bytes);
  ctx.fpscr.disableFlushMode();
  katamari::Fence();
  const uint8_t* pm = ::banjo::hot::Raw(base, mtx);
  auto m = [&](uint32_t off) { return ::rex::ppc::SingleToDoublePure(katamari::LoadW(pm + off)); };
  const double m0 = m(0), m4 = m(4), m8 = m(8), m16 = m(16), m20 = m(20), m24 = m(24), m32 = m(32), m36 = m(36),
               m40 = m(40);
  if (!s.Note(out, bytes)) [[unlikely]] return true;
  ::banjo::hot::Card(base, out, bytes);
  const uint8_t* pi = ::banjo::hot::Raw(base, in);
  uint8_t* po = ::banjo::hot::Raw(base, out);
  using katamari::Round1;
  for (uint32_t k = 0; k < uint32_t(n); ++k, pi += 12, po += 12) {
    const double x = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi));
    const double y = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi + 4));
    const double z = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi + 8));
    const double ox = Round1(std::fma(m0, x, Round1(std::fma(m32, z, REX_FMULS(m16, y)))));
    katamari::StoreW(po, katamari::FloatBits(ox));
    const double oy = Round1(std::fma(x, m4, Round1(std::fma(m36, z, REX_FMULS(m20, y)))));
    katamari::StoreW(po + 4, katamari::FloatBits(oy));
    const double oz = Round1(std::fma(y, m24, Round1(std::fma(m8, x, REX_FMULS(z, m40)))));
    katamari::StoreW(po + 8, katamari::FloatBits(oz));
  }
  return true;
}

}  // namespace n_8275A828

// sub_8275AA88 - r6 points at r4 transformed by the 4x4 matrix at r3 into r5, divided by w unless w is exactly 1
// (a |w| below 5e-7, 0x350637BD, is replaced by +5e-7). The original has four loops for this (pairs, a tail, a four-
// wide loop the pairs leave nothing for, and a separate path when r4 == r5); each point gets the same arithmetic,
// x = R(R(fma(m0, x, R(fma(m32, z, R(m16 y))))) + m48) and so on, w = R(R(fma(m44, z, R(fma(m12, x, R(m28 y))))) +
// m60), then 1/w as fdivs and three fmuls. The pairs loop reads both points before it writes either, so outputs that
// overlap the inputs could differ from one point at a time: declined, as are r4 == r5 (the other path) and an output
// over the matrix. Stack: the prologue's two saves (the link register's low word at r1 - 8, the zeroed local r31 at
// r1 - 16) and |w| of the last point divided at r1 - 160 (the original clears the sign through memory), as the
// original leaves them. Flush mode off, as the original switches it at once. Scratch not reproduced (hand.txt): r7-r10,
// f1-f13, lr (the epilogue's mtlr reloads the saved word).
namespace n_8275AA88 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kTransform)) [[unlikely]] return false;
  const uint32_t mtx = ctx.r3.u32, in = ctx.r4.u32, out = ctx.r5.u32, sp = ctx.r1.u32;
  const uint32_t n = ctx.r6.u32;
  if (in == out || n > 65536u) [[unlikely]] return false;
  const uint32_t bytes = 12u * n, frame = sp - 160u;
  if (!detail::Straight(mtx, 64) || !detail::Straight(frame, 160)) [[unlikely]] return false;
  if (n && (!detail::Straight(in, bytes) || !detail::Straight(out, bytes) || katamari::Overlap(in, bytes, out, bytes) ||
            katamari::Overlap(out, bytes, mtx, 64) || katamari::Overlap(out, bytes, frame, 160))) [[unlikely]]
    return false;
  s.Hand(kTransform, bytes);
  ctx.fpscr.disableFlushMode();
  katamari::Fence();
  const uint8_t* pm = ::banjo::hot::Raw(base, mtx);
  auto m = [&](uint32_t off) { return ::rex::ppc::SingleToDoublePure(katamari::LoadW(pm + off)); };
  const double m0 = m(0), m4 = m(4), m8 = m(8), m12 = m(12), m16 = m(16), m20 = m(20), m24 = m(24), m28 = m(28),
               m32 = m(32), m36 = m(36), m40 = m(40), m44 = m(44), m48 = m(48), m52 = m(52), m56 = m(56),
               m60 = m(60);
  const double one = ::rex::ppc::SingleToDoublePure(REX_RO_U32(0x82000B38, 0x3F800000));
  const double eps = ::rex::ppc::SingleToDoublePure(REX_RO_U32(0x820050F4, 0x350637BD));
  if (!s.Note(frame + 144u, 16) || (n && !s.Note(out, bytes))) [[unlikely]] return true;
  uint8_t* pf = ::banjo::hot::Raw(base, frame);
  katamari::StoreW(pf + 152, uint32_t(ctx.lr));  // stw r12,-8(r1): the link register as the original reads it
  std::memset(pf + 144, 0, 8);                   // std r31,-16(r1): the local r31, never set before
  if (!n) return true;
  ::banjo::hot::Card(base, out, bytes);
  const uint8_t* pi = ::banjo::hot::Raw(base, in);
  uint8_t* po = ::banjo::hot::Raw(base, out);
  using katamari::Round1;
  bool temp_written = false;
  uint32_t temp_bits = 0;
  for (uint32_t k = 0; k < n; ++k, pi += 12, po += 12) {
    const double x = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi));
    const double y = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi + 4));
    const double z = ::rex::ppc::SingleToDoublePure(katamari::LoadW(pi + 8));
    const double ox = REX_FADDS(Round1(std::fma(m0, x, Round1(std::fma(m32, z, REX_FMULS(m16, y))))), m48);
    const double oy = REX_FADDS(Round1(std::fma(m4, x, Round1(std::fma(m36, z, REX_FMULS(m20, y))))), m52);
    const double oz = REX_FADDS(Round1(std::fma(m8, x, Round1(std::fma(m40, z, REX_FMULS(m24, y))))), m56);
    double w = REX_FADDS(Round1(std::fma(m44, z, Round1(std::fma(m12, x, REX_FMULS(m28, y))))), m60);
    if (w == one) {
      katamari::StoreW(po, katamari::FloatBits(ox));
      katamari::StoreW(po + 4, katamari::FloatBits(oy));
      katamari::StoreW(po + 8, katamari::FloatBits(oz));
      continue;
    }
    temp_bits = katamari::FloatBits(w) & 0x7FFFFFFFu;
    temp_written = true;
    if (::rex::ppc::SingleToDoublePure(temp_bits) < eps) w = eps;
    const double inv = REX_FDIVS(one, w);
    katamari::StoreW(po, katamari::FloatBits(REX_FMULS(ox, inv)));
    katamari::StoreW(po + 4, katamari::FloatBits(REX_FMULS(oy, inv)));
    katamari::StoreW(po + 8, katamari::FloatBits(REX_FMULS(oz, inv)));
  }
  if (temp_written) {
    if (!s.Note(frame, 4)) [[unlikely]] return true;
    katamari::StoreW(pf, temp_bits);
  }
  return true;
}

}  // namespace n_8275AA88

// sub_8276E3D8 - the integration step of r6 bodies (96 bytes each in the array whose address is the word at r3 + 16):
// the byte at r3 + 169 toggled (which of two 32-byte slots of a body is read and which is written), then for each body
// seven fmadds (the position and the rotation, slot += rate x f1 with the rates at +64 ... +88), the written rotation
// normalised (four fmuls by 1 / sqrt of the sum of squares, as fsqrts / fdivs) and f2 stored at +28 of the written
// slot. The original reads the array's address again after every store (a store could change it); here it is read
// once, after checking that the array does not overlap the object's first 170 bytes (else declined); the toggle must
// leave 0 or 1 (the two slot offsets it selects), else declined. The prologue's save of the local r31 (zero) at r1 - 8
// is written as the original writes it. Flush mode off before the loop as in the original (when there is a body).
// Scratch not reproduced (hand.txt): r4-r10, f9-f13.
namespace n_8276E3D8 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kIntegrate)) [[unlikely]] return false;
  const uint32_t obj = ctx.r3.u32, sp = ctx.r1.u32;
  const int32_t n = ctx.r4.s32;
  if (n > 65536) [[unlikely]] return false;
  if (!detail::Straight(obj, 170) || !detail::Straight(sp - 8u, 8)) [[unlikely]] return false;
  const uint8_t* po = ::banjo::hot::Raw(base, obj);
  const uint8_t toggled = uint8_t(po[169] ^ 1u);
  if (toggled > 1u) [[unlikely]] return false;
  const uint32_t arr = katamari::LoadW(po + 16);
  const uint32_t bytes = n > 0 ? 96u * uint32_t(n) : 0u;
  if (n > 0 && (!detail::Straight(arr, bytes) || katamari::Overlap(arr, bytes, obj, 170) ||
                katamari::Overlap(arr, bytes, sp - 8u, 8))) [[unlikely]]
    return false;
  // The two slot offsets (the original's two paths on the low byte of r7 give the same pair, swapped).
  const bool by_toggle = (ctx.r7.u32 & 0xFFu) != 0;
  const uint32_t write_slot = (by_toggle ? toggled : uint32_t(toggled == 0)) << 5;
  const uint32_t read_slot = (by_toggle ? uint32_t(toggled == 0) : toggled) << 5;
  s.Hand(kIntegrate, bytes);
  if (!s.Note(sp - 8u, 8) || !s.Note(obj + 169u, 1) || (n > 0 && !s.Note(arr, bytes))) [[unlikely]] return true;
  std::memset(::banjo::hot::Raw(base, sp - 8u), 0, 8);  // std r31,-8(r1): the local r31, never set before
  ::banjo::hot::Card(base, obj + 169u, 1);
  ::banjo::hot::Raw(base, obj)[169] = toggled;
  if (n <= 0) return true;
  ::banjo::hot::Card(base, arr, bytes);
  ctx.fpscr.disableFlushMode();
  katamari::Fence();
  const double one = double(std::bit_cast<float>(uint32_t(REX_RO_U32(0x82000B38, 0x3F800000))));
  const double dt = ctx.f1.f64;
  const double f2 = ctx.f2.f64;
  uint8_t* body = ::banjo::hot::Raw(base, arr);
  auto F = [](const uint8_t* p) { return double(std::bit_cast<float>(katamari::LoadW(p))); };
  using katamari::Round1;
  for (int32_t k = 0; k < n; ++k, body += 96) {
    const uint8_t* rd = body + read_slot;
    uint8_t* wr = body + write_slot;
    // Positions (three) and rotation (four): slot + rate x dt, in the original's order.
    for (uint32_t c = 0; c < 7; ++c) {
      const double v = Round1(std::fma(F(body + 64 + 4 * c), dt, F(rd + 4 * c)));
      katamari::StoreW(wr + 4 * c, katamari::FloatBits(v));
    }
    const double q1 = F(wr + 4);
    double sum = REX_FMULS(q1, q1);
    const double q0 = F(wr), q2 = F(wr + 8), q3 = F(wr + 12);
    sum = Round1(std::fma(q0, q0, sum));
    sum = Round1(std::fma(q2, q2, sum));
    sum = Round1(std::fma(q3, q3, sum));
    const double inv = REX_FDIVS(one, REX_FSQRTS(sum));
    katamari::StoreW(wr, katamari::FloatBits(REX_FMULS(q0, inv)));
    katamari::StoreW(wr + 4, katamari::FloatBits(REX_FMULS(q1, inv)));
    katamari::StoreW(wr + 8, katamari::FloatBits(REX_FMULS(q2, inv)));
    katamari::StoreW(wr + 12, katamari::FloatBits(REX_FMULS(q3, inv)));
    katamari::StoreW(wr + 28, katamari::FloatBits(f2));
  }
  return true;
}

}  // namespace n_8276E3D8

// sub_824E97F0 - the game's sine: f1 = sin(f1). The angle reduced by a multiple of 2 pi (the sign of the angle
// copied onto pi by rlwimi, the integer part of (angle + pi) / 2 pi by fctiwz through the stack, fnmsubs), then the
// odd powers up to the 23rd in three vectors and three vmsum4fp128 with the coefficient vectors at 0x82000AA0 ...
// 0x82000AC0, added as the original adds them. The original's flush-mode switches are kept (the reduction with the
// mode off, the polynomial with it on: its smallest terms are denormals for ordinary small angles, where the mode
// decides the result); what is gone is the traffic: the stack temporaries are written once, as they end (three
// vectors at r1 - 48, r1 - 32, r1 - 16; the stack pointer must be 16-aligned, else declined), and nothing goes
// through the context. Scratch not reproduced (hand.txt): r9, r10, f12, f13, v0, v9-v13.
namespace n_824E97F0 {

template <class S>
[[gnu::always_inline]] inline bool Native(PPCContext& __restrict ctx, uint8_t* base, S& s) {
  if (!katamari::KOn(kSine)) [[unlikely]] return false;
  const uint32_t sp = ctx.r1.u32;
  if ((sp & 15u) || !detail::Straight(sp - 48u, 48)) [[unlikely]] return false;
  s.Hand(kSine, 48);
  // stfs f1,-48(r1); the sign of the stored single onto pi (0x40490FDB) by rlwimi.
  const uint32_t angle_bits = std::bit_cast<uint32_t>(::rex::ppc::DoubleToSingleAnyMode(ctx.f1.f64));
  const uint32_t pi_bits = (angle_bits & 0x80000000u) | 0x40490FDBu;
  const double two_pi = ::rex::ppc::SingleToDoubleAnyMode(REX_RO_U32(0x820050B8, 0x40C90FDB));
  ctx.fpscr.disableFlushMode();
  katamari::Fence();
  const double angle = ctx.f1.f64;
  const double signed_pi = double(std::bit_cast<float>(pi_bits));
  const double shifted = double(float(signed_pi + angle));
  const double turns = REX_FMULS(shifted, double(std::bit_cast<float>(uint32_t(REX_RO_U32(0x82005148, 0x3E22F983)))));
  const double whole = double(float(double(int64_t(int32_t(::rex::ppc::fctiwz(turns))))));
  const double r = double(float(-std::fma(whole, two_pi, -angle)));
  const double r2 = REX_FMULS(r, r);
  const double r3 = REX_FMULS(r2, r);
  // The three stfs with the mode still off (a denormal single would be flushed by the conversion with it on).
  uint32_t w1 = katamari::FloatBits(r), w2 = katamari::FloatBits(r2), w3 = katamari::FloatBits(r3);
  katamari::PinW(w1);
  katamari::PinW(w2);
  katamari::PinW(w3);
  alignas(16) uint8_t pw[16];
  katamari::StoreW(pw, 0x3F800000u);  // QuietSingleBits(1.0f)
  katamari::StoreW(pw + 4, w1);
  katamari::StoreW(pw + 8, w2);
  katamari::StoreW(pw + 12, w3);
  const simde__m128 c0 = simde_mm_castsi128_ps(REX_RO_V128(0x82000AA0, 0x3F800000BE2AAAABull, 0x3C088889B9500D01ull));
  const simde__m128 c1 = simde_mm_castsi128_ps(REX_RO_V128(0x82000AB0, 0x3638EF1DB2D7322Bull, 0x2F309231AB573F9Full));
  const simde__m128 c2 = simde_mm_castsi128_ps(REX_RO_V128(0x82000AC0, 0x274A963CA317A4DAull, 0x1EB8DC789A3B0DA1ull));
  simde__m128 v0 = simde_mm_castsi128_ps(katamari::LoadV(pw));
  ctx.fpscr.enableFlushModeUnconditional();
  katamari::Pin(v0);  // the vector arithmetic below after the switch
  simde__m128 v13 = simde_mm_castsi128_ps(simde_mm_shuffle_epi32(simde_mm_castps_si128(v0), 0xAA));
  v0 = simde_mm_mul_ps(v0, v0);
  v0 = simde_mm_mul_ps(v0, v13);
  const simde__m128 v9 = simde_mm_castsi128_ps(simde_mm_shuffle_epi32(simde_mm_castps_si128(v0), 0x0));
  const simde__m128 v10 = simde_mm_dp_ps(v0, c0, 0xFF);
  v13 = simde_mm_mul_ps(v9, v13);
  v0 = simde_mm_mul_ps(v0, v13);
  v13 = simde_mm_mul_ps(v0, v13);
  v0 = simde_mm_dp_ps(v0, c1, 0xFF);
  v13 = simde_mm_dp_ps(v13, c2, 0xFF);
  alignas(16) uint8_t out[48];
  katamari::StoreV(out, simde_mm_castps_si128(v13));
  katamari::StoreV(out + 16, simde_mm_castps_si128(v0));
  katamari::StoreV(out + 32, simde_mm_castps_si128(v10));
  const double a = ::rex::ppc::SingleToDoublePure(katamari::LoadW(out + 16));
  const double b = ::rex::ppc::SingleToDoublePure(katamari::LoadW(out));
  const double c = ::rex::ppc::SingleToDoublePure(katamari::LoadW(out + 32));
  ctx.f1.f64 = ::rex::ppc::AddSingleAnyMode(::rex::ppc::AddSingleAnyMode(b, a), c);
  if (!s.Note(sp - 48u, 48)) [[unlikely]] return true;
  ::banjo::hot::Card(base, sp - 48u, 48);
  std::memcpy(::banjo::hot::Raw(base, sp - 48u), out, 48);
  return true;
}

}  // namespace n_824E97F0

}  // namespace banjo::hand

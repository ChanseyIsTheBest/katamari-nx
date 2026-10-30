# Beautiful Katamari port: notes

This file records the port: what each step found, with its evidence. The project starts from banjo-nx (Banjo-Kazooie:
Nuts & Bolts, `D:\Banjo kazooie port\banjo-nx`, read-only): its SDK fork, app, tools, renderer and post-codegen
passes. banjo-nx's docs are the reference for everything taken from it; comments that say "banjo-nx", "sa2-nx",
"peggle2-nx" or "sasrt-nx" record findings from those ports.

The user's request (2026-10-30): port Beautiful Katamari to the PC and the Switch with ReXGlue, banjo-nx as the base,
the Switch the main goal; work uninterrupted until 20:00 AEST.

## Decisions (2026-10-30, made without asking, as the request said)

| What | Decision | Why |
|---|---|---|
| Game files | `D:\beautifulkatamari\Beautiful Katamari [NTSCU]\Beautiful Katamari [NTSCU].iso`, extracted to `D:\beautifulkatamari\game` | The request named `D:/kingdomkeflings`, but that folder holds A Kingdom for Keflings and another session's `keflings-nx` port (left alone). The Katamari ISO is in this session's working folder. |
| Port folder | `D:\beautifulkatamari\katamari-nx` (git, local commits only) | next to the game, like `D:\kingdomkeflings\keflings-nx` |
| Ryujinx data folder | `D:\New folder\ryu-data\katamari` | the port's own, as the request asks |
| `app/assets` | a junction to `D:\beautifulkatamari\game` | the manifest reads `assets/default.xex`; no second 2.7 GB copy on a D: drive with 7-20 GB free |
| Internal project name | `renut` (as banjo-nx) | banjo-nx's CMake, post-codegen passes and renderer glue name `renut_*` files and targets; the NRO, its title and the SD folder are katamari-nx |
| First boot | the SDK's emulated GPU (`gpu_plugin = "xenos"`), Renut compiled out | separates CPU / kernel problems from renderer problems (peggle2-nx's order of work); Renut's hooks are at N&B's addresses and must be moved first |
| Renut's hooks | compiled but not registered until moved (`KATAMARI_RENUT_HOOKS`, `banjo_renut_select.h`) | an N&B address is an unrelated function in this game |
| Banjo-only sources | not built: cheats / mid-asm hooks (`hooks.cpp`), title update, DLC, `banjo_me_hooks.cpp`, XAudio natives, hand natives, hot copies, shader harvest | each is written for N&B's addresses; `src/katamari_keys.cpp` defines the keys the kept files read (off) |
| Host shader tools | glslangValidator, SPIRV-Cross, uam copied from banjo-nx's `out/` | built from the same sources this tree carries (`tools/deko3d_build_tools.sh`) |

## The game (2026-10-30)

- ISO: XGD2, 1,392 files, 9 directories, 2.68 GB (`tools/extract_iso.py`, from peggle2-nx).
- `default.xex`: title **4E4D07DC**, media `2C479157`, version 0.0.0.4, built 2007-09-02; base `0x82000000`, entry
  `0x824C1A28`, image `0x011F0000` (18.8 MB); AES + LZX. Code range (generated `REX_CODE_BASE / SIZE`)
  `0x82210000-0x82A05350`.
- XDK: every static library **2.0.5632** (XAPILIB, D3D9, D3DX9, XGRAPHC, XBOXKRNL, XNET, XAUD, XMEDIA, XONLINE, XHV,
  LIBCMT). Older than N&B's 7645 and Feeding Frenzy 2's 6534; between Feeding Frenzy 1's 2571 and FF2's 6534 in the
  table of the "XDK versions of our Xbox 360 ports" doc.
- Data: `d02.dat`-`d05.dat` archives (2.2 GB), WMV movies (`data/movie`, Japanese and US), `.nps` sound streams
  (`data/sound/bgm`, `data/sound/stream`), `alchemy.ini`, `$SystemUpdate`. No other code module in the data.

## Code generation (2026-10-30)

- Config: `app/renut_config.toml`, banjo-nx's codegen keys that need no per-game audit (CR / CTR / XER / reservation
  as locals, skip_msr, dform split, the fcti / fp-mode / copy / div / rotate forms, dcbt prefetch). The register
  diet (non_volatile_as_local, elide_lr_stores, non_argument_as_local) waits for this game's audits.
- **First run:** three "target not in any function", each a function reached only by a tail jump from a thunk
  (`app/config/renut_funcs.toml`, with the instructions): `0x825387E8`, `0x8266DEA0`, `0x82408488`.
- **Second run:** three "Unresolved conditional branch" to `li r3,0; blr` tails the scanner left outside their
  function: `0x822AC5E8` declared as its own function (the branch becomes a conditional tail call, same result);
  `0x82685520` (`cmplwi cr6,r4,0; bne cr6,0x82685530`, then the tail) declared over the whole body to `0x826855C8`
  (the generator writes no fall-through into a next function and refuses overlapping declarations: with the stub
  ending at the tail its r4 == 0 path returned without r3 = 0).
- **Pointer sweep** (`tools/pointer_fixpoint.py`, from peggle2-nx, adapted to this tree): pass 1, 18,307 gaps, 262
  candidates, **111 accepted** (vtables, callback tables, `lis/addi` callbacks; `app/overrides.toml`), rejected 127
  tail branches into function bodies, 15 without code, 4 beyond their gap, 4 without a terminator, 1 fragment; pass 2
  no new candidates (fixpoint).
- 269 generated files.

## Renut's renderer at Katamari's addresses

Not started yet. Inputs: Feeding Frenzy 2's method (XDK 6534: state tables found by the defaults at stride 12, the
rest by field signature, `tools/renut_hook_remap_ff2.py`), Zuma's for older resource headers (XDK 2135), peggle2-nx's
`tools/renut_hook_remap.py` and `renut_device_offsets.py`.

To do once Renut runs (from sasrt-nx, 2026-10-30): Renut writes no resolve result back to guest memory, so game code
that reads a GPU result on the CPU (auto-exposure, glare visibility) gets 0. sasrt-nx's fix is
`sasrt_renut_resolve_readback` ("D:\sonic transformed\sasrt-nx\tools\renut_patches\sasrt_edits.py"). Check whether
Katamari locks resolve destinations on the CPU (log Renut's `LockSurface` on resolve destinations). plume's four
deko3d fixes of the same message (Zcull invalidate braces, MSAA sample positions, alpha-to-coverage dither,
primitive restart) are already in this tree's plume (copied from banjo-nx).

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
| Shipped `renut.toml` | 720 lines handheld and docked, no dynamic resolution, the game's MSAA (`disable_msaa = false`), shadow scale 1x, post-processing at the game's precision, no extra frame cap | pixel-identical by default; banjo-nx's 648/864 lines, dynamic resolution, MSAA off and 1080 docked are `configs/*.toml` for the user (`tools/make_katamari_tomls.py`) |
| Four-core forwarder | program id `05D6EA1333487100` (banjo-nx's is `...7000`), NPDM `tools/forwarder/katamari_main.npdm` | its own id so the two NPDMs never collide; the user makes the forwarder |

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
  - **Seven of those were wrong** and are removed (`app/overrides.toml`, with the reason): their "built at" address was
    0x14 bytes before them, the `lis/addi` that builds a jump table's base right after its `bctr`; they were case code
    of their parent (the codegen then reported "Jump target ... unresolved at bctr" for 0x8230DCD8, 0x8240305C,
    0x8255BCD0, 0x82707584). A port taking this tool should drop "built at" candidates 0x14 after their build.
- **First boot stop:** "Call to invalid or unregistered function at guest address 0x82689770": a 10-instruction table
  accessor in a gap between two registered ones, reached through a pointer built elsewhere. **Gap sweep**
  (`tools/gap_sweep.py`, new): candidates are the first word of every gap after its padding and the word after each
  `blr` / `bctr` inside a gap, not within 0x800 bytes after a jump-table `bctr` (an indexed load feeding `mtctr`:
  the first try without that rule took 2-instruction switch cases for functions and split their parents); the
  checks are the pointer sweep's. **85 functions**, fixpoint after one pass; the codegen reports no unresolved branch.
- **Jump tables** (`tools/switch_check.py`, from peggle2-nx): 768 switches; truncated 0, wrong targets 0, missed 0;
  23 odd shapes generated as indirect jumps (to look at if one of them stops a run).
- **Second boot stop:** "exception C000001D" (the generated `__builtin_trap` of an out-of-range switch) in
  `sub_8222E438`: a dispatch through a table of four function starts (`0x82A196A8`) indexed by the word at `0x82B095E0`.
  The generator had made it a switch on the loaded *address* (r11) with cases 0-3. Fixed with a `[[switch_tables]]`
  entry on r10 (the index already times 4: the four targets at 0, 4, 8 and 12), `app/overrides.toml`. The other six
  "no bound in the code" tables of `switch_check.py` switch on their index registers (checked in the generated code).
- **setjmp / longjmp** by shape: setjmp `0x827EB5D0` (saves f14-f31 then r13-r31 into the buffer at r3, after a hook
  word check), longjmp `0x827EB3B0` (about 20 callers in 0x828A-0x829A, image decoders' error exits).
- 269 generated files.

## Renut's renderer at Katamari's addresses (2026-10-30)

banjo-nx's Renut is written for N&B's **title update 3** (XDK 2.0.7978): its hooks are at the update's addresses and
its device offsets are the update's (some differ from the base game's: IndexBuffer 12436 in the base, 12444 in the
update). Every comparison below is against banjo-nx's own generated tree (`D:\Banjo kazooie port\banjo-nx\app\generated`)
and image dump, never the base game.

### The hooks (`tools/renut_hook_remap_katamari.py`, `out/renut_map/remap_report.txt`)

322 of Renut's 354 hooks are at Beautiful Katamari's functions; the other 32 compile but are registered nowhere
(`BANJO_RENUT_UNHOOKED_*`, `banjo_renut_select.h`):

- **222 state functions from the state tables** (`tools/renut_state_tables.py`): N&B's records {getter, setter,
  default} at `0x82D8A948` (101 render states) and `0x82D8AE28` (20 sampler states); Katamari's are at **`0x82AD8E48`**
  and **`0x82AD9308`**, found by N&B's defaults at stride 12, every record paired by its default (same count, same
  order).
- **60 by field signature** (`tools/renut_sigmatch.py`: the multiset of instructions with registers removed, D-form
  displacements and immediates kept, imports by name; weighted Jaccard), score >= 0.75 and 0.05 ahead of the next.
  Examples: DrawVertices `sub_824D9D08` 0.90, DrawIndexedVertices `sub_824DA0F0` 0.87, Clear `sub_824F3370` 0.84,
  Resolve `sub_824F12B8` 0.78, SetVertexShader / SetPixelShader `sub_824D4930` / `sub_824D4630`, the lock and
  surface-description functions at 1.00.
- **40 by hand** (`tools/renut_hook_map_katamari.txt`, each with its evidence): CreateDevice `sub_824D0598` (allocates
  the 0x5700 device, calls the device init `sub_824E18D0`), Present `sub_824E9428` (calls
  SynchronizeToPresentationInterval, Resolve, KickOff, Swap), Swap `sub_824E8E90` (the only VdSwap caller),
  GetDeviceCaps `sub_824E6410` (copies 304 bytes from `0x820FC1F8`), SetGammaRamp / SetPWLGamma `sub_824DE218` /
  `sub_824DE2F8` (called in N&B's order by InitializePresentationParameters), the surface-level getters read in the
  image (GetCubeMapSurface `sub_824D2160` keeps the face argument), and the ring and fence functions.
- **Not hooked**: BeginExport / EndExport (XDK 2.0.5632 has no memexport), the command-buffer functions, 
  DrawIndexedVerticesUP, the one-instruction thunks, D3D_IsFusion, a few ambiguous ones. The XDK is linked
  statically, so a function with no counterpart is usually not in the image at all (the command-buffer recorder and
  runner have none: BeginCommandBuffer's best candidate is BeginTiling at 0.27). Two the game does use, checked by
  hand: EndIndexedVertices is `sub_824D9CE8` (`Ring = VerticesRingEnd`, then a kick unless Flags has 0x80): on
  Renut's devices, which carry the no-hardware flag 0x80, the game's code does exactly what Renut's hook does, so it
  stays unhooked; BeginIndexedVertices `sub_824D97B8` (0.85 against N&B's `sub_8263EDA0`, called once from
  `0x8271F7C8`) is not hooked in banjo-nx either.
- **Tiling: XDK 2.0.5632 keeps the full public forms** (N&B's update dropped the unused arguments).
  BeginTiling is **`sub_824EFBC8`** (it stores the count at TileCount and the targets at TilingSurfaces, where the
  zones below put N&B's fields; the game calls it at `0x8271E62C`): BeginTiling(Flags r4, Count r5, pTileRects r6,
  pClearColor r7, ClearZ f1, ClearStencil r9). It clears with ClearZ / ClearStencil and keeps them for EndTiling,
  stores Flags at +0x337C, sets Flags3 0x20 (and 0x10 with Flags bit 2) when Flags has bit 1, and skips its first
  clear when Flags has bit 0 or Flags3 has 0x20. EndTiling `sub_824F0150` (ResolveFlags r4, pResolveRects r5,
  pDestTexture r6, pClearColor r7, ClearZ f1, ClearStencil r9, pParameters r10) resolves every tile but the last
  with ResolveFlags | 0x100 (a colour target bound) | 0x200 (a depth surface bound) and BeginTiling's clear values,
  the last with the caller's values. Renut's two hooks became `BeginTilingImpl` / `EndTilingImpl` with those
  arguments; raw hooks read them from the registers (`d3d/hooks/draw.cpp`).
- The profiler's frame boundary is at Present (N&B's was its `appMainDraw` hook, not built here).

### The device layout (`tools/renut_field_diff.py`, `renut_field_pairs.py`, `renut_device_offsets_katamari.py`)

The displacements of 288 paired functions, N&B's update against Katamari:

| N&B update offsets | Katamari | Evidence |
|---|---|---|
| 0 .. 10943 | the same | ring, state-call tables, fetch constants, the register packets, fences, flags (158 pairs identical) |
| 10944 .. 11827 | -12 | 11012 -> 11000; VertexDeclaration 11824 -> 11812 (SetVertexDeclaration) |
| 11828 .. ~13470 | -16 | 11832 -> 11816 (XenonGpu_Initialize), the render-state block 11856.. -> 11840.., IndexBuffer / RenderTargets / DepthStencil / streams / textures / predication / tiling, VerticesRingEnd 13444 -> 13428, InKickOff 13232 -> 13216 |
| ~13490 .. 16800 | -24 | CommandBuffer 13516 -> 13492, PresentInterval 13596 -> 13572, the presentation copy 13544 -> 13520, auto surfaces 14824.. -> 14800.., ring fields 14836.. -> 14812.., critical sections 14944 / 14972 -> 14920 / 14948 (RtlInitializeCriticalSection calls), SwapCount 16560 -> 16536, 16708 -> 16684, SideBuffer 16728 -> 16704 |
| 21532 | 21508 | Swap |
| 21544 .. 21592 | -28 | display mode 21544 / 48 / 52 / 56 -> 21516 / 20 / 24 / 28 (InitializePresentationParameters, by source), the -1 words 21560 / 64 -> 21532 / 36 and perf frequency / period 21588 / 92 -> 21560 / 64 (the device init) |
| 21624 .. 21632 | -32 | Swap |
| 23348 .. 23956 | -1708 | the 41 -1 words 23380 -> 21672 (`li r11,41; addi r10,r31,23380` / `addi r11,r3,21672; li r10,41`), gamma 23952 / 56 -> 22244 / 48 |
| 23976 .. 23980 | -1716 | BehaviorFlags 23980 -> 22264 (InitializeApiState, XenonGpu_Initialize) |
| device size 0x5E00 | 0x5700 | Katamari allocates and clears 0x5700 (`li r3,22272`, `sub_824D0240`); N&B allocates 0x5E80, clears 0x5E00 |

165 lines of Renut rewritten (`out/renut_map/offsets_report.txt`); every literal fell in a zone. Three of them were
not fields but 64-byte chunk boundaries of the replay's delta compare and census (`backend/queued.cpp`
`kDeltaPrivateUnreadEnd`, two lines of `render/draw.cpp`: 11776), caught by queued.cpp's static assertions: put
back to 11776, still the last boundary before Katamari's vertex declaration (+11812).

### Guest data (`tools/renut_data_refs.py`)

| What | N&B update | Katamari | Evidence |
|---|---|---|---|
| XDK build qword (device +10880) | `0x82DA3478` | `0x82AD93F8` | holds 0x0002000016004000 (2.0.5632.0) |
| VdGlobalDevice import record | `0x820007F8` | `0x82000750` | both resolve to the same kernel variable (0xFFCAE000); D3D_LockResource, D3DResource_Destroy |
| D3DCAPS9 | `0x82161B30` | `0x820FC1F8` | GetDeviceCaps's 304-byte copy |
| full viewport | `0x82001744` | `0x820FAEB4` | EndTiling, D3D_SetSurfaceInfo |
| XPhysicalAlloc's no-large-pages flag | `0x82DA3778` | `0x8315DFE0` | Katamari's XPhysicalAlloc at `0x824C1C30` (same instructions) |
| format table | `0x82005200` | `0x820FA9D0` | seven D3D functions agree |
| HiZ owner | `0x82E108A0` | `0x8315E830` | CreateSurface, D3DResource_Destroy |
| XMemFree | `sub_822089F0` | `sub_824CAB98` | the hardware-device init frees with `lis r4,0xB1800000; li r3,0; bl` |
| D3D_IsFusion's word, the two export skip words | `0x82DC67D4`, `0x82E1A9E4/8` | none | not in XDK 2.0.5632: 0, and the reads guarded |

### First runs on the PC (2026-10-30, Renut on Vulkan)

- `renut2` (minimised, so no swapchain: no pacing, about 2,000 frames a second): 150 s, the generated-code
  self-checks 0 differences, XMA audio decoding, 16 draws dropped on the device's own declaration (fixed:
  `DeviceDeclarationLocked`, from peggle2-nx, at +0x2F98) and five run-time shaders captured, two of which pass the
  fog interpolator Renut's XenosRecomp did not declare (fixed: `{ Fog, 0 }` appended to its interpolators).
- `renut3` (visible window): 60 frames a second by vsync, render thread 0.3-0.5 ms and GPU 1.4-3.2 ms a frame, no
  draw dropped, no shader missing: the boot screen (the Prince on white), the save menu ("New Game", the button
  prompts), the "Start a new game?" dialog, all drawn as expected.

### What the game decides (porting-guide-renderer.md section 2; census of the menus, gameplay to follow)

| Question | Beautiful Katamari (menus) | N&B |
|---|---|---|
| Where draws come from | direct calls: 0 command-buffer recordings run; 83-84 % DrawVerticesUP | 98.5 % recorded command buffers |
| Where resources come from | 100 % of vertex streams from a Create hook; 100 % of bound textures from a Create hook or SetTextureHeader; no locks | 5 % / 26 % through creation calls |
| How state is set | through the hooked setters (87-88 % of draws change nearly every field group: UI draws) | 99.9 % hookable setters |
| Memexport | none (no export functions in XDK 2.0.5632) | nearly every draw |

So the record-time items built for N&B's recorded buffers (resource / geometry objects at record time, the replay
stream, record lookups: A/B 45-50) have little or nothing to act on in the menus; the per-draw fast path, pipelines
by content, pushed constants and whole-buffer copies apply as they are.

### Shaders

The image holds 118 shader containers (92 vertex, 26 pixel; 111 unique by XXH3): `tools/renut_shader_dump.py` (from
peggle2-nx) seeded `out/renut_shader_dump` with them. Shaders the game makes at run time from its data are captured
by Renut on the PC (its capture folder) and added by the next build.

The Switch's inputs (`tools/switch_shaders.ps1`, banjo-nx's steps): Renut's XenosRecomp with the early-alpha pass and
the deko3d SPIR-V output, SPIRV-Cross to GLSL, banjo-nx's 27 GLSL passes, uam. First run on the 111: all compile
(86 vertex shaders, median 31 KB of DKSH, largest 154 KB; 25 pixel shaders, median 1 KB), the package 6 MB of GLSL
-> 518 KB (`app/out/renut_switch/deko3d_glsl.bin`), the Switch copy of the cache 23 KB + 527 KB of SPIR-V. banjo-nx's
hash-keyed passes (skin-once, light-branch, null-ps, gpr32 lists) name N&B's shaders and change none of these.

To do once Renut runs (from sasrt-nx, 2026-10-30): Renut writes no resolve result back to guest memory, so game code
that reads a GPU result on the CPU (auto-exposure, glare visibility) gets 0. sasrt-nx's fix is
`sasrt_renut_resolve_readback` ("D:\sonic transformed\sasrt-nx\tools\renut_patches\sasrt_edits.py"). Check whether
Katamari locks resolve destinations on the CPU (log Renut's `LockSurface` on resolve destinations). plume's four
deko3d fixes of the same message (Zcull invalidate braces, MSAA sample positions, alpha-to-coverage dither,
primitive restart) are already in this tree's plume (copied from banjo-nx).

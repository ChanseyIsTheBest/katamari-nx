# Smaller game files: H.265 movies, Opus audio, LZ4 / ZXC packing

A guide for ports built on the ultimate base (written by sasrt-nx, 2026-11-23, at the user's request: "write a document
how to re-encode audio and video and how to compress files with l4z and zxc compression ... include as many different
audio format and video format details from the games we have worked on"). It gathers what twelve ports found
between 2026-11-10 and 2026-11-23: rr6-nx, culdcept-nx, sasrt-nx, sa2-nx-ult, banjo-nx, vivapinata-nx, kameo-nx,
nights-nx, blur-nx, futari-nx / ketsui-nx / guwange-nx, and the base's own Katamari.

Every figure says where it comes from. "Measured" means a port ran it on its own files. "Estimate" means it was worked
out from another port's result.

**Units.** Several ports' tools divide by 1,048,576 but print "MB". Those figures are given here as MiB (Culdcept, Viva,
the Cave games, RR6's archives). Every other size is decimal MB. The percentages hold either way.

## Contents

1. The idea, and the rules every port kept
2. What it saves: every game's figures
3. The files the base reads, and which one answers for NAME
4. Packing: LZ4 and ZXC
5. Movies to H.265
6. Audio to Opus
7. The web installer's side
8. Traps, all in one list
9. Where the code is, and what the base still lacks
10. Credits

---

## 1. The idea, and the rules every port kept

**The game reads its own files, byte for byte.** Every re-encoded file is a stand-in, NAME.x, that the SDK shows to the
game as NAME:

- the same size;
- every header, index, table, padding and audio byte the game reads as it was;
- only the picture data or the sound data replaced.

Where the game would decode a picture or a sound, a tag marks the spot, and a hook or the XMA emulation fetches the
pixels or samples from the H.265 or the Opus instead. The game's own player, mixer, loops, seeks and timing stay
untouched; only the pixels and the samples come from elsewhere. The same works for a file inside an archive (an AFS or a
PAK2), because no offset moves.

Two paths work differently:

- **`.opcm`** has no tag and no hook: the file handle decodes the Opus back to PCM as the game reads.
- **ASF movies (WMV3 or VC-1)** are rewritten rather than stood in for. The installer writes the H.265 frames into the
  ASF file itself, under its own name. The file is not the same size (NiGHTS's: a few % smaller than its muxer's),
  and the decoder hook tells H.265 from WMV by the first frame.

**Game data is never distributed.** The player's browser makes every file from the player's own disc (the web
installer), or a PC tool makes it for a test. Nothing a tool writes from game data is committed, uploaded or shared.

**The order of work, by what it saves:**

1. Movies. They are usually the largest files: H.265 at about a third of the original.
2. Raw PCM music. Opus cuts it by about 90 %.
3. Compressed audio (XMA, ADX). Opus saves 30-70 % (Viva's XMA 38 %, NiGHTS's ADX 68 %).
4. Packing whatever is left that compresses.

Measure before building anything. Sort the files by their first 4 bytes, compress a few slices of each kind (zlib level
1 for a quick look, then the real LZ4 and ZXC), and leave a kind alone when it is already compressed.

**The rules the installers use** (each installer's own; they differ):

- **The 95 % rule** (the Cave ports' installers, `futari-nx-installer/lib/media.js`). A file that packs to 95 % or
  more of its size is copied as it is. banjo-nx-installer and vivapinata-nx-installer pack every file that gets
  smaller at all (Viva's `.pkg` at 99.2 %).
- **The 5 % rule** (sasrt-nx-installer, `zxc_min_saving` 0.05). A `.xmop` is wrapped in ZXC only when ZXC saves at
  least 5 %. vivapinata-nx-installer always LZ4-packs its `.xmop` (2.0 % smaller).
- **Not smaller, not changed.** A sound whose Opus is not smaller than its XMA stays XMA. Kameo keeps a movie as Bink
  when it cannot reach its 40 dB floor under 90 % of its Bink video.
- **Small files stay plain.** Files under 4 KB are not packed (banjo-nx, vivapinata-nx).
- **`default.xex` (and `.xexp`) is never packed.** The NRO loads and maps the module itself.
- **Re-encoded movies are ZXC-packed.** This is the user's rule for the ports helped on 2026-11-21/22 (RR6, SASRT, SA2,
  Blur, NiGHTS, Viva), even where ZXC saves only 1 %. The exceptions:
  - Culdcept's 1.0.1 leaves its movies out, at the user's word;
  - banjo-nx-installer (branches `media-pack`, `release-1.0.3`) packs LZ4 only and leaves its ASF movies unpacked;
  - kameo-nx-installer has no packing yet.

---

## 2. What it saves: every game's figures

| Game | Before | After | How | Source |
|---|---|---|---|---|
| Sonic & All-Stars Racing Transformed | about 4.93 GB (4.6 GiB) | about 2.33 GB (2.2 GiB) | archives LZ4, movies H.265 + ZXC, Wwise XMA2 to `.xmop` | measured per file (sasrt-nx-installer, Node); the "4.7 to 2.2 GB" of its README mixes units |
| Ridge Racer 6 | the disc's files, 6.2 GB (rr6-nx's figure) | about 3.4 GB on the card | RRM archives LZ4, rest ZXC, PCM music `.opcm`, SFD movies H.265 + ZXC | rr6-nx 1.0.2 README; each part measured, no total recorded |
| Culdcept Saga | archives 3,273 MiB, movies 1,069 MiB, sound 290 MB | 1,875 MiB, 414 MiB, 171 MB | archives LZ4, effects ZXC, WMV3 to H.265 in the ASF, XMA1 to `.xmo` | measured (the installer's full test in Chrome; played on the console) |
| Sonic Adventure 2 | 2,400 MiB | 1,166 MiB (926 without the interviews) | movies H.265 (ZXC), ADX to `.adxo`, rest LZ4 | measured (sa2-nx-ult installer in Chromium) |
| Banjo-Kazooie: Nuts & Bolts | 3,178 MB packable + 820 MB music bundle + 1,155 MiB movies | 2,071 MB + 361 MB + 427 MiB | LZ4, XACT PCM music to `.opcm`, WMV3 to H.265 | measured (banjo-nx, Ryujinx and Chromium) |
| Viva Piñata | 4,978.8 MiB | 3,243.4 MiB | WMV3 to H.265 + ZXC, XACT 2 XMA to `.xmop` (+ LZ4), everything else LZ4 | measured (vivapinata-nx) |
| Kameo | 5,788 MB | about 3,217 MB (the user's "middle" choice) | Bink 1 to H.265 (a 40 dB floor), XACT v23 XMA1 to `.xmop` at 48 / 96 kbit/s; levels already compressed | measured per part (kameo-nx) |
| NiGHTS into Dreams | 2,141 MB | about 0.75 GB (estimate) | packing 1,093 to 311 MB and ADX 450 to 143 MB (both measured); 516 MB of VC-1 to H.265 + ZXC at about 40 % (estimate: the other ports' H.265 movies came to 34-39 %) | nights-nx PC runs |
| Blur | about 5.7 GB | not measured as a whole | level render and physics LZ4 (231 to about 99 MB, measured); level streams ZXC; 31 Bink 1 movies (1,343 MB) to H.265 + ZXC; `root_streaming.pak`'s XMA to `.xmop` | blur-nx-installer 1.0.1 |
| Ketsui | 841 MiB | about 170 MiB | textures LZ4 (742 to 99 MiB), XMA2 music to `.xmo` / `.xmop` | textures measured, music estimate |
| Mushihimesama Futari (+ DLC) | 810 MiB (+ 263) | about 525 MiB (+ 216) | XACT PCM music to `.opcm`, gallery art ZXC | packing measured, music estimate |
| Guwange | 107 MiB | 78 MiB | ZXC on the ROMs and two of the three DSD files; the DLLs and xex as they are | measured |
| Katamari (the base's game) | d02-d05.dat 2,143 MB | 943 MB | LZ4 | measured (base port 61) |

---

## 3. The files the base reads, and which one answers for NAME

The base's SDK reads all of these but `.adxo`: `.lz4b` version 1 since port 61, the rest since port 259. Each one is
NAME's stand-in in a host folder (`host_path_entry.cpp`, `host_path_device.cpp`):

| Stand-in | Holds | Magic | Spec | Made by |
|---|---|---|---|---|
| `NAME.lz4b` | NAME in LZ4 (version 1) or ZXC (version 2) blocks | `RXLZ4BLK` ... `RXLZ4END` | `sdk/include/rex/filesystem/lz4_blocks.h` | `tools/lz4b_pack.py` (LZ4), `tools/zxcb_pack.cpp` (ZXC), installer `lib/lz4b.js` |
| `NAME.hevc`, `NAME.hevc.lz4b` | a movie whose pictures are H.265 (any container: SFD, USM, Bink) | `RXSFDH01` | `sfd_hevc.h` | rr6-nx `tools/hevc_sfd_pack.py`, sasrt-nx `tools/hevc_usm_pack.py`, blur-nx `tools/bik_hevc_pack.py`; installers `lib/sfdh.js`, `usmh.js`, `biks.js` |
| `NAME.opcm` | a file with 16-bit PCM ranges as Opus | `RXOPCM01` | `opus_pcm.h` | rr6-nx `tools/opus_pcm_pack.py`, banjo-nx `tools/xwb_opcm_pack.py`, installer `lib/opcm.js` |
| `NAME.xmo` | one XMA file whose frames are Opus | `RXXMAOP1` | `xmo.h` | culdcept-nx-installer `lib/xmo.js` |
| `NAME.xmop`, `NAME.xmop.lz4b` | a package of many XMA sounds whose frames are Opus | `RXXMAPK1` | `xmo.h` | sasrt-nx-installer `lib/xmop.js`, `test/xmop_node.mjs` |
| `NAME.adxo` | CRI ADX streams whose blocks are Opus | `RXADXOP1` | sa2-nx-ult `sdk/include/rex/filesystem/adx_opus.h` | sa2-nx-ult `tools/adx_opus_pack.mjs` (+ `adx_opus_lib.mjs`). **Not in the base yet** (section 9) |

Re-encoded movies wrapped in ASF (WMV3 or VC-1 to H.265) need no stand-in format of their own. The installer writes
the H.265 frames into the ASF file itself, so the result is just NAME, or NAME.lz4b when ZXC-packed. The game's
decoder hook tells H.265 from WMV by the first frame.

**Which file answers:**

- **A plain NAME beside any stand-in wins.** HostPathEntry warns "... are both in the game folder: the plain file is
  used (delete one of them)". So an installer must not leave the original next to what it made, and a test folder must
  move the original aside.
- **Among the stand-ins, a re-encoded file answers before NAME.lz4b** (AnswerRank: rank 0 for `.opcm`, `.xmo`,
  `.xmop`, `.hevc` and their `.lz4b` forms; rank 2 for a plain `.lz4b`). RR6 found on the console that the release's
  `NAME.lz4b` hid the new `NAME.opcm` and `NAME.hevc`, so an install unzipped over an older one would have played the
  old files. The order is fixed now.
- **A full scan reads every stand-in's header.** Culdcept's took 22.8 s on the console, and 39.6 s in Ryujinx (10.8 s
  there before its `.xmo` files). Viva's took 14.0 s on the PC against 4.6 s.
- **The VFS index can serve later starts only if it records two sizes** for a stand-in: its size on the SD and the
  size it stands for.
  - culdcept-nx's index (its own version 2, `kNotPacked`), nights-nx's copy of it, and futari-nx's version 3 do. With
    it, Culdcept's later starts take 237 ms.
  - **The base's index did not until port 264** (2026-11-23). Its version 2 ("children in AnswerRank's order") kept
    one size per file, the size the game sees, and ValidateDirectory compared that with the SD listing's size, so a
    folder with any stand-in never validated and **every start did the full scan**. sasrt-nx, rr6-nx, blur-nx,
    vivapinata-nx, sa2-nx-ult and kameo-nx have that index until they take port 264.
  - **Port 264's version 4** keeps each file's SD size and, for a stand-in, its kinds, the size it stands for and the
    game's name for it, for every kind of port 259. In Ryujinx, Katamari's start from the index took 309.8 ms, against
    13.8 s for the scan.
- **Stand-ins are read-only.** Open refuses writes, and OpenMapped refuses.

---

## 4. Packing: LZ4 and ZXC

### The container

`NAME.lz4b` (little-endian; `lz4_blocks.h`):

- a 32-byte header: `"RXLZ4BLK"`, u32 version (1 = LZ4 blocks, 2 = ZXC blocks), u32 block size, u64 the size of the
  file it holds, u32 block count, u32 0;
- the blocks back to back: LZ4 block format (version 1) or `zxc_compress_block` output (version 2). A block whose
  stored size equals its length is stored as it is;
- the index: a u32 per block, its stored size;
- a 16-byte footer: u64 the index's offset, `"RXLZ4END"`. The footer at the end lets a browser write the file as a
  stream.

A read decompresses only the blocks it covers:

- runs of whole blocks go straight into the caller's buffer (up to 4 MB of the file per host read) and are not kept;
- a block decoded for a part-block read is kept (`cached_block_`), so small reads in order unpack each block once.

A movie's frames, read in order, therefore unpack one 512 KB block about every dozen frames, on the read-ahead thread.

The libraries:

- LZ4 1.10.0 (`sdk/thirdparty/lz4`) and ZXC 0.15.0 (`sdk/thirdparty/zxc`), in the C library `rexblockcodecs`.
- `-fno-char8_t` is for C++ only.
- The reader and its codecs are outside the LTO and PGO targets (rr6-nx's building.md).

### The settings

| Codec | Settings | Decode on the console | Use it for |
|---|---|---|---|
| LZ4 HC level 9, 256 KB blocks (version 1) | `lib/lz4b.js` CODECS `lz4` | about 590 MB/s | what a load screen reads: archives, levels, textures |
| ZXC level 7, 512 KB blocks (version 2) | CODECS `zxc` | about 200 MB/s (about 3 times slower than LZ4) | what is read while playing, or rarely: movies, gallery art, DLC, ROMs |
| LZ4 HC, 64 KB blocks (version 1) | vivapinata-nx's CODECS `lz4s` (game.json list `lz4_small`) | not measured | a large file read in small, random pieces (Viva's 739 MiB `debug_pack.bin`) |

ZXC is usually a little smaller, but not always. **Measure each kind of file and pick per kind** (`codecFor` in the
installers):

| Game, files | LZ4 HC 9 | ZXC 7 | Picked |
|---|---|---|---|
| Ketsui, uncompressed textures (742 MB) | 99.2 MB (13.4 %) | 109.7 MB (14.8 %) | LZ4: runs of zeros favour it, and it decodes 3x faster |
| NiGHTS, `.ddm` / `.bin` / `.csb` / `.dat` / `.csa` (1,093 MB) | 330 MB | 312 MB | ZXC for the first four, LZ4 for the tiny `.csa` (311 MB in all) |
| Banjo, `Bundle/50` (3,285 MB) | 2,439 MB (74.2 %) | 2,378 MB (72.4 %) | LZ4 (ZXC's ~70 MB more, over Bundle/50 and the CAFF bundles together, was not worth a second format then) |
| Futari, gallery art and wallpapers (561 MB) | 486 MB (86.6 %) | 450 MB (80.2 %) | ZXC (read when the gallery opens) |
| Futari DLC bins (263 MB) | 233 MB | 216 MB | ZXC |
| Guwange, the whole folder (107 MiB: about 74 MiB of ROMs and DSD files, plus 33 MiB of DLLs and xex stored as they are) | 79 MiB | 78 MiB | ZXC (read at a mode's start); DSD200 kept by the 95 % rule |
| SA2, the game files the installer packs (932 MB) | 502 MB | 486 MB | LZ4 ("just LZ4 compress everything besides the ZXC compress the H.265 videos") |
| Blur, level render and physics (231 MB) | about 99 MB | 5 MB smaller | LZ4 (3x faster to decode, read at loading screens) |
| RR6, RRM archives (3,032 MiB) | 2,425 MiB | | LZ4; everything else ZXC |
| SASRT, `GameAssets.M00` / `Frontend.M00` / `GameData.M00` | 1,390 / 272 / 0.33 MB | | LZ4; `AvatarAssetPack`, `nxeart` ZXC |
| Viva, `debug_pack.bin` (Rare CAFF, 739 MiB) | 574 MiB (77.7 %, 64 KB blocks) | | LZ4 64 KB |

**What does not pack** (all measured at 95-105 %):

- Rare's CAFF containers:
  - Viva's `bundles_packages/*.pkg` hold zlib streams and pack to 99.2 %; Viva's installer LZ4-packs them anyway.
  - Kameo's `.lvl` and `.mdl` pack to 99.9 % (zlib inside is inferred from Viva's).

  A smaller file would mean inflating each stream and recompressing it inside the container.
- Banjo's XMemCompress bundles (first bytes `0F F5 12 ED`).
- Encrypted or compressed XEX2 modules (Guwange's game-mode DLLs, Futari's `media/Module/*.xex`).
- Already-compressed movies and audio.

### Tools

PC tools, for tests and checks (in the base's `tools/`):

- `python tools/lz4b_pack.py pack IN OUT.lz4b [--level 9] [--block 262144]`, `check PACKED ORIGINAL`, `info PACKED`.
  It needs pip's lz4 module.
- `tools/zxcb_pack.cpp` packs ZXC on every core: `zxcb_pack IN OUT.lz4b [level 7] [block KB 512]`. Its header comment
  has the clang64 build line.
- `tools/test_lz4_blocks.cpp` checks either version.

The installer's side (rr6-nx-installer, culdcept-nx-installer, sasrt-nx-installer):

- `lib/lz4b.js` (`pack()` with codec `lz4` or `zxc`), `wasm/lz4.*`, `wasm/zxc.*` and the pack worker pool.
- What to pack:
  - rr6-nx-installer: `codecFor`, hard-coded in `lib/lz4b.js`;
  - culdcept-nx-installer: the `lz4` / `zxc` lists in `game.json`;
  - sasrt-nx-installer: those lists plus `asis` and `skip`.
- `test/pack_node.mjs` makes the page's files on a PC. Its arguments differ:
  - sasrt-nx-installer's and vivapinata-nx-installer's: `node test/pack_node.mjs OUT [--codec zxc] FILES`;
  - rr6-nx-installer's: `OUT FILE...`;
  - culdcept-nx-installer's: `GAME OUT [--only REGEX] [--link-rest]`.
- vivapinata-nx-installer's `test/lz4_measure.mjs` measures every file in memory without writing anything.

---

## 5. Movies to H.265

### The method, whatever the container

Three of the five movie paths keep the container byte for byte (SFD, USM, Bink). The two ASF paths (WMV3 and VC-1)
rewrite the ASF with the H.265 frames in it.

1. **The stand-in, `NAME.hevc`** (RXSFDH01, `sfd_hevc.h`; container-agnostic):
   - segments of NAME, back to back: stored bytes, zeros (they cost nothing on disk), or tag bytes;
   - a table of the coded pictures' display indices;
   - the H.265 frames in display order, Annex B, no B frames, each led by an access unit delimiter, a keyframe
     carrying the parameter sets every 120 frames.

   The game reads its movie with each picture's video data replaced by a 10-byte tag and zeros:

   - **The tag:** `A5 5A C3`, the movie id in 3 bytes, the display index in 3 bytes (7 bits each, high bit set,
     most significant first), then `0x80 | (sum of those 6 bytes & 0x7F)`. It contains no zero byte, so it can never
     form MPEG's `00 00 01` start code. The checksum also guards the read where a hook is not given the data's end
     (Kameo's Bink call).
2. **The hook** sits where the game's decoder takes a picture's data. It finds the tag, decodes nothing, and writes the
   H.265 picture of that index where the game's decoder would have put its pixels.
3. **The decoder on the Switch is nxvideo** (culdcept-nx's `tools/switch/nxvideo`):
   - devkitPro's switch-ffmpeg 7.1 built with averne's nvtegra backend, linked into one object with only `nxv_*`
     global;
   - in the base it is the static library `renut_nxvideo`, with `RENUT_NXVIDEO=1` when devkitPro's FFmpeg has
     nvtegra. Only a port whose movie hook calls `nxv_*` takes it into its NRO.
4. **The re-encoded movie goes into ZXC blocks:** `NAME.hevc.lz4b`, or, for an ASF movie, the file's own name plus
   `.lz4b` (NiGHTS's `NAME.wmv.lz4b`; Viva's movies have no extension, so `NAME.lz4b`). Culdcept's 1.0.1 leaves its
   movies unpacked (0.8 %, the user's word). The container's own bytes compress, the H.265 does not:

   | Movies | ZXC saves on the re-encoded file |
   |---|---|
   | RR6's SFDs (their ADX audio kept) | 10.3-11.5 % (LZ4: 8.9-10.9 %); about 100 MB in all |
   | SA2's 31 SFDs | 14.4 % (184.6 to 157.9 MB) |
   | SASRT's USMs | 1.4-9 % (Drifting 1.4, Gameclear 1.7, Logos 9) |
   | Blur's Bink | 2.7 % (Blur's figure, relayed by kameo-nx; Kameo has not measured its own) |
   | NiGHTS's ASF (VC-1 to H.265) | 1-16 % |
   | Viva's ASF (WMV3 to H.265) | 0.55 % (2.9 of 533 MiB) |
   | Culdcept's ASF | 0.8 % |

   Re-encoded audio gains little: RR6's `.opcm` music 0.3 %, `.xmop` packages 2-5 % (Viva: LZ4 2.0 %, ZXC 2.5 %),
   which is under the 5 % rule. Viva packs its `.xmop` with LZ4 anyway, because LZ4 decodes so fast.

### Encoding: settings that worked

**In the browser (WebCodecs `VideoEncoder`):**

```js
{ codec: 'hvc1.1.6.L120.B0',          // Main profile, level 4
  width, height, bitrate, bitrateMode: 'variable', framerate: fps,
  hardwareAcceleration: 'prefer-hardware', latencyMode: 'quality',
  hevc: { format: 'annexb' } }
encoder.encode(frame, { keyFrame: shown % 120 === 0 });
```

- The pictures come from FFmpeg decoders built to WebAssembly: `wasm/mpeg1` (FFmpeg 4.4's MPEG-1, matches native
  ffmpeg picture for picture), `wasm/wmv3`, `wasm/vc1` (`--enable-decoder=vc1`) and `wasm/bink` (`binkvideo`,
  byte-exact against native ffmpeg).
- A browser without an H.265 encoder keeps the original movies.
- Test pages in Chrome or Edge. The Claude app's built-in Chromium has the H.265 encoder but refuses nested workers,
  so the pack worker pool never starts there (vivapinata-nx).

**On a PC (ffmpeg):**

- libx265:
  ```
  -c:v libx265 -preset slow -b:v BITRATE
  -x265-params bframes=0:keyint=120:min-keyint=120:scenecut=0:open-gop=0:repeat-headers=1:aud=1
  ```
- NVENC:
  ```
  -c:v hevc_nvenc -preset p7 -tune hq -rc vbr -b:v BITRATE -maxrate 2xBITRATE -bf 0 -g 120 -forced-idr 1
  -no-scenecut 1 -bsf:v hevc_metadata=aud=insert
  ```
- Either way: `-pix_fmt yuv420p -fps_mode passthrough -an`, output `-f hevc`.
- These are the SFD, USM and Bink packers' settings (`hevc_sfd_pack.py`, `hevc_usm_pack.py`, `bik_hevc_pack.py`).
- culdcept-nx's ASF packer, `tools/hevc_movie.py` (used by banjo-nx and vivapinata-nx too), has its own:
  - NVENC (its default):
    ```
    -c:v hevc_nvenc -bf 0 -aud 1 -g 120 -rc vbr -b:v K -maxrate 1.5xK -bufsize 2xK -preset p7 -tune hq
    -multipass fullres -forced-idr 1
    ```
    with K = 35 % of the source's video bitrate, at least 200 kbit/s;
  - libx265: `-crf 28 -preset medium`, with `bframes=0:aud=1:repeat-headers=1:keyint=120:min-keyint=120`.

**The bitrate:**

- **35 % of the original video's bitrate** for MPEG-1, WMV3 and VC-1 (Culdcept's rule; 35 % of the video keeps
  about 41 % of a file).
- **Bink 1 needs less, but per movie:**
  - Blur: 22 % (`tools/bik_hevc_pack.py --fraction 0.22`). Its installer also keeps at least 300 kbit/s
    (`reencodeBink`'s `minKbps = 300` in `lib/biks.js`; the PC tool has no such option).
  - Kameo found a fixed 22 % too little for some movies (Cut_Kameo_Rare_Logo: 33.2 dB). It used a constant-quality
    ladder instead, with options added to kameo-nx's copy of the tool: `--encoder hevc_nvenc --cq 32 --mean-psnr 40
    --keep-above 0.9`. That means a mean PSNR floor of 40 dB, and a movie that cannot reach it under 90 % of its Bink
    video stays Bink.
- **Rejected options:**
  - A deblocking filter before encoding (RR6). H.265's own deblocking does most of it, `pp=de` smears drawn artwork,
    and the heavy filters add 25-40 minutes to a browser install.
  - AV1 (Culdcept, measured on credit_us):
    - SVT-AV1 was 14.2 MB against H.265's 33.3 MB at the same 41 dB, but it cannot be made in a browser, and dav1d
      on one console thread took 18.7 ms a frame with 13 % of frames late;
    - the browser's AV1 was 22.3 MB at 39.2 dB: a lower quality, and judged worse than H.265 for its size;
    - H.265 on NVDEC took 4.6 ms a frame with none late. All movies stayed H.265.

**Check every movie:**

- `--check` on the SFD, USM and Bink packers: every byte outside the video data as in the original, every tag in
  place, and PSNR against ffmpeg's decode of the original, every Nth frame. The ASF packer (`hevc_movie.py`) writes no
  tags and has no `--check`: those ports compared with ffmpeg's psnr filter and a script of their own (Viva's
  `asf_hevc_check.py`: the same media objects, times, header and packet size).
- PSNR results across the ports:

  | Movie | Mean PSNR (lowest) |
  |---|---|
  | SASRT credits | 42.1 dB (34.6) |
  | SASRT Drifting | 49.8 (42.1) |
  | SASRT Gameclear | 44.8 (42.2) |
  | SASRT Logos | 66.1 (49.3) |
  | SA2 e0401_m | 52.6 |
  | Viva | 37.7-44.7 |
  | Banjo narrations | 41.8 |
  | NiGHTS SEGA_HD | 44.6 |
  | Culdcept credits | 41.4 |
  | Kameo, at its 40 dB floor | 40 |

### Playing them on the Switch: what every port had to do

| What | Why | Key or code |
|---|---|---|
| Raise the decoding thread to priority 0x2C around `nxv_decode`, and put it back after | The game's threads run at 0x3B and Horizon does not time-slice them, so a thread woken from NVDEC waits for a core. RR6 went from 12-14 ms to 3.4 ms a 720p picture | culdcept `culdcept_movie_decode_priority`, rr6 `rr6_hevc_decode_priority`, sasrt `sasrt_hevc_decode_priority`, blur `blur_hevc_decode_priority` |
| Read the frames ahead on a thread of their own (45 frames, 1.5 s) | Reading each frame from the SD on the game's thread cost RR6 4.6-6.1 ms while the game streamed the movie's audio. With the read-ahead, 1,203 of 1,204 frames came from memory and took 0.04 ms each | rr6 `Prefetch` / `AheadLoop` / `StartAhead` / `TakeFrame` |
| Make that thread a libnx thread: `threadCreate` at 0x2C, core -2, 256 KB stack, `threadStart` | A `std::thread` started from the game's thread aborted the process (svcBreak in Ryujinx); a pthread made from an SDK thread hung the process (found in `masseffect_deferred_recording.cpp`) | |
| Copy the picture out with the VIC into an NV12 picture whose planes and pitches are 256-byte aligned, in whole pages, invalidated after the VIC writes | nvtegra copies with the VIC only into a 256-aligned picture; otherwise it falls back to a CPU un-tiling that cost Culdcept 18 ms a frame, against 1.4-1.9 ms now. nvtegra's YUV420P copy is broken (its U plane comes out 255) | nxvideo defaults; `culdcept_movie_vic_copy`, `culdcept_movie_verify` |
| Hold NVDEC at its maximum clock (979 MHz) and the VIC at its maximum while a movie plays | nvtegra's DFS lowers NVDEC's clock every 20 frames to the bitrate it saw; the game asks for a frame only after showing the last, so the clock falls until a frame takes a display frame | `culdcept_movie_max_clock`, `banjo_movie_max_clock` |
| Test in Ryujinx with the software decoder | Ryujinx has no H.265 on NVDEC ("Nvdec Decode: Unsupported codec Hevc"); nxvideo counts the empty pictures it gets back as decoded, so "0 damaged" means nothing there. Only the console checks NVDEC | `rr6_hevc_nvdec`, `sasrt_hevc_nvdec`, `blur_hevc_nvdec`, `sa2_hevc_nvdec` = false |
| Run `tools/direct_calls.py --redo` (or `tools/post_codegen.py`) after adding a `REX_HOOK_RAW` | The generated callers call `__imp__` directly, so a new hook is never called; bulk machine copies of a caller also bypass it (RR6 turned two off). Blur's first green console run was this | |
| Mark the written rows' store cards | So the renderer uploads the frame buffer again | blur, kameo |
| Remove bulk hot copies of hooked functions | Viva's first link failed on duplicate symbols | `localized_bulk.txt`, the generated `.inc` |
| Show black pictures, at the movie's own times, when no decoder can open | NiGHTS's title movie held the game on the PC when every frame answered "no picture" | |

Measured console cost per picture:

- RR6, 720p: about 5.4 ms on the game's thread in all (decode 4.7-5.0 ms, copy 0.5 ms).
- Culdcept: 4.6 ms a frame (9.3 ms worst) in the av1bench benchmark of all 6,470 frames of the credits; in the game,
  2.6 ms for a 60 fps narration (send 1.2 ms, VIC copy 1.4 ms).
- Blur, Bink: 2.9-3.2 ms on NVDEC, plus 0.5 ms copy.
- RR6's 480x272 course previews: 3.7 ms a picture in all (reading, NVDEC and the copy), before the priority fix.

### Container by container

#### CRI Sofdec `.sfd`: an MPEG-1 program stream, MPEG-1 video and ADX audio (Ridge Racer 6, Sonic Adventure 2)

| Game | Movies | Libraries |
|---|---|---|
| RR6 | 37 SFDs (CRI CRAFT 2.28 / 2.96): 720p openings and 480x272 course previews | MWSFD 3.71f / CRI MPV 1.976 |
| SA2 | 11 cutscenes, 640x480 29.97 fps with ADX (368 MB); 20 clips, 320x240 with no audio (37 MB) | MWSFD 5.54, CRI MPV 2.081, CRI SFD 2.183 |

The packer (`tools/hevc_sfd_pack.py`, installer `lib/sfdh.js`; both write the same bytes given the same H.265
frames):

- It replaces each picture's slice data with one slice header (quantiser byte `0x08`, quantiser scale 1), the tag and
  zeros. Zero runs are stored as zero segments.
- The display index is a picture's rank among the display times. SA2 found GOPs whose temporal references skip
  numbers.

The hooks:

- CRI MPV's one-slice function finds the tag and skips the macroblocks. The slice's first byte holds the header's last
  2 bits, so the tag is at that byte + 1. SA2's function is `sub_82901C40`: GetChunk into mpv+5288, slice decoder at
  mpv+5084.
- MWSFD's `mwPlyGetCurFrm` writes the H.265 picture into the frame (frm +0 the Y plane, +12 / +16 width and height).
  SA2's is `sub_828E06F8`; RR6's offsets are in the header of `rr6_hevc_movies.cpp`.

The B-picture trap (SA2):

- mpv+4992 / +4996 / +5000 are a B picture's backward reference.
- Its own planes are at +5008 / +5012 / +5016 (Y at +5016).
- The first hook tagged the reference, so the wrong index was shown and the B picture stayed untagged. The fix also
  logs a `[hevc]` warning for any frame shown without a tag.

#### CRI Sofdec2 `.usm`: CRI chunks, MPEG-1 video and ADX audio (Sonic & All-Stars Racing Transformed)

- **The files.** Four USMs, MPEG-1 1280x720 at 29.97-30 fps, 649 MB (619 MiB):
  - credits 4:41 (11.6 Mbit/s);
  - Drifting 1:31 (15 Mbit/s);
  - Gameclear 0:15;
  - Logos 0:12.

  Their audio is ADX 48 kHz stereo; Logos's codec is one ffprobe does not name. Audio stays as it is.
- **The container.** CRID, `@SFV` (video), `@SFA` (audio), `@CUE` and other chunks. The packer (`tools/hevc_usm_pack.py`,
  installer `lib/usmh.js`) finds the MPEG-1 elementary stream inside the `@SFV` payloads and cuts each picture's
  slices there. The tag and zeros take exactly the slice bytes' place, so every chunk keeps its size and offset.
- **The hook.** SASRT already decoded the MPEG-1 natively (`sasrt_usm_ffmpeg.cpp` hooks the slice loop). The slice loop
  takes the tagged slice; FinishPicture finds the tag and writes the H.265 picture of that index into the coded
  picture's planes. It decodes forward and keeps up to 5 pictures for the B pictures MPEG asks for out of order.
- **Keys:** `sasrt_hevc_movies`, `sasrt_hevc_nvdec`, `sasrt_hevc_decode_priority`, `sasrt_hevc_log`.
- **Results:** 649 to 238 MB (619 to 227 MiB). credits went from 426 to 160 MB, at 35 % of the bitrate.

#### ASF with WMV3 (Culdcept Saga, Banjo-Kazooie: Nuts & Bolts, Viva Piñata, SA2's interviews, Ketsui)

| Game | Movies | Audio |
|---|---|---|
| Culdcept | 39, WMV3 1280x720, 30 fps (the CI_00-02 intros, the credits) or 60 fps (narrations, endings), no B pictures; 1,068.8 MB | WMA 2 |
| Banjo N&B | 52, WMV3 1280x720 30 fps (43 at 30/1, 9 at 30000/1001), 19.1 minutes | WMA 2 / WMA Pro (some 5.1) |
| Viva | 48, WMV3 1280x720 (30 at 23.976 fps, 8 at 29.97, 6 at 59.94, 4 at 30), 28 minutes | WMA 2 stereo; one WMA Pro 5.1, three silent |
| SA2 interviews | WMV3 1920x1080, 8-9.5 Mbit/s, 658 MB (interview.wmv 5:56, interview_jp.wmv 4:32) | WMA 9 Pro 48 kHz stereo |
| Ketsui | `5pb.wmv`, `title.wmv`: WMV3 1280x720, 18 s, 7.1 MB. Left as they are (futari-notes: "Movies stay as they are"); H.265 would save about 4.6 MB (an estimate) | not recorded |

**The hooks: the XDK's XMedia WMV9 wrapper.** The same library build in Culdcept, N&B and Viva: the same instructions
but for registers, the same offsets and status codes.

| Role | Culdcept | N&B | Viva |
|---|---|---|---|
| decode | sub_8259EE00 | WmvideoC9DecDecode | sub_825799F0 |
| get output | sub_8259EE60 | WmvideoC9DecGetOutput | sub_82579A50 |
| flush | sub_8259ECF8 | WmvideoC9DecReset | sub_825798E8 |
| destroy | sub_8259ED50 | WmvideoC9DecClose | sub_82579940 |
| GetData | sub_8259EA38 | WMVDecCBGetData_HANDLER | sub_82579628 |
| the stream node's payload fetch (mode 2) | sub_8256FEE8 | WMDecCBGetData | sub_8254AAD8 |

- **The wrapper's layout:** C+32 V; V+32 width, +36 height, +68 the WMV9 decoder, +72 pts, +84 / +88 the sequence
  header, +108 the frame's I420 size. The output is written into the player's I420 buffer (Y at pitch W, then U and V
  at pitch W / 2).
- **To find them in a new game,** compare its generated code with Culdcept's or N&B's. Match by instruction counts
  (ratio 0.92-1.00) and memory offsets, and by how far apart the functions lie.
- **Three modes** (`*_wmv_ffmpeg`): 0 the game's decoder, 1 FFmpeg's WMV3 (or nxvideo for H.265), 2 both, compared
  picture for picture. Culdcept, N&B and Viva found FFmpeg's WMV3 **bit-identical** to the game's decoder: 0 samples
  differ in Y, U and V.
- **The SDK's FFmpeg** compiles the WMV3 / VC-1 sources (`sdk/thirdparty/CMakeLists.txt`), but a port must switch
  them on. `sdk/thirdparty/FFmpeg/config_masseffect_wmv3.h` (a masseffect-nx addition, nights-notes) is in the base's
  tree, but the base's `config.h` (in the tree since port 190) does not include it: add `#include
  "config_masseffect_wmv3.h"` after the platform headers. The game's own decoder took about 97 % of a console core in
  Culdcept.

**SA2's interviews are not the WMV9 wrapper.** They go through XMedia2's own player:

- XMedia2's setup treated the SDK's stub `XamGetDashBackstackData` as a failure, so the player stayed null and SA2 called
  it anyway. The crash was reproduced on the `ultimate` branch before 5e0e8d41; 1.0.6 has the same stub but was not run.
- sa2-nx-ult's `sa2_interviews.cpp` replaces the player: the ASF is read by the port, H.265 or WMV3 goes through
  nxvideo, the picture is 1280x720 ARGB in three textures SA2 draws itself, and WMA Pro is decoded by the SDK's FFmpeg
  on the video's clock.

**The installer rewrites the ASF** (culdcept-nx-installer's `lib/asf.js`, `lib/movies.js`, `movie_worker.js`;
`tools/hevc_movie.py` writes the same bytes). The XDK player paces by the packets' send times and loses a split audio
object's sound, so:

- each audio object stays whole in one packet. One that does not fit starts the next packet, and the video goes on
  filling the packet before;
- each packet's send time is the original time of the bytes it starts with (for a re-encoded frame: of the same
  fraction of the original's bytes), never earlier than the packet before's;
- each packet's duration is the time to the next.

The first writer split audio objects, and the intros played slowly and silently. The "loud stutter noise" at the
start was most likely half objects fed to the WMA decoder.

#### ASF with VC-1 advanced profile, `WVC1` (NiGHTS into Dreams)

- **The files.** 22 `afs/*.wmv` (516 MB): VC-1 advanced profile (sequence header with start codes), 1280x720 or 640x480
  at 29.97 fps, with B pictures and WMA Pro audio. Culdcept's are WMV3 without B pictures.
- **The player** is the same XDK library as Culdcept's, a later build:
  - decode sub_82453B20, get output sub_82453B80, flush sub_82453A18, destroy sub_82453A70, GetData sub_82453758,
    status sub_82453678, the stream node's fetch sub_82427B40;
  - V at C+32 with Culdcept's fields.

  **Its statuses differ:** 0 OK, 2 no more output (Culdcept 9), GetData's last chunk 6 (Culdcept 10).
- **FFmpeg needs `ff_vc1_decoder`** added to `sdk/thirdparty/ffmpeg-overlay/codec_list.c`, and the installer's wasm
  `--enable-decoder=vc1`.
- **What the game's decoder shows:**
  - after each frame, the newest picture FFmpeg has given (both hold a reference back for the B pictures);
  - at a movie's first frame, that frame's own picture (FFmpeg gives it only when drained: a decoder of its own,
    `FirstPicture`);
  - for a skipped frame (a 1-byte payload), no picture: S_OK, size 0.

  With these rules FFmpeg matched the game picture for picture and time for time over four movies.
- **The installer** writes, in each non-skipped frame's place, the H.265 picture of what the game's decoder would show,
  keeps skipped frames, and ZXC-packs the result (`NAME.wmv.lz4b`).

#### RAD Bink 1, `BIKi` (Blur, Kameo)

**The files:**

| Game | Movies | Audio |
|---|---|---|
| Blur | 31 `Ui/movies/*.bik`, 1280x720, 1,343 MB | |
| Kameo | 64 `fmv/*.bik` (2,631 MB, 66.5 minutes): 37 at 720x480 and 27 at 1280x720, all 29.97 fps. Video 94.1 %, audio 5.9 %, headers 0.5 MB | 2-12 Bink audio tracks each (`binkaudio_dct`, mono 44.1 kHz: the languages) |

Bink audio stays as it is: Opus for it would need a second hook, for 5.9 % of the movies.

**The layout** (little-endian; `tools/bik_hevc_pack.py`):

- 0 `"BIKi"`, 4 the file's size - 8, 8 the frame count, 12 the largest frame, 20 / 24 width and height, 28 / 32 the
  frame rate's numerator and denominator, 36 video flags, 40 the audio track count T;
- then T x 4 bytes, T x 4 bytes (each track's rate and flags) and T x 4 bytes (each track's id);
- then (frames + 1) u32 offsets (bit 0: a keyframe);
- a frame: for each audio track a u32 size and that many bytes, then the video data to the frame's end.

Bink 1 has no reordering: the frame index is the display order. That is simpler than MPEG-1's B pictures.

**The hook** is Bink 1's video decode into the frame buffers:

- Blur: `sub_82AFF648`, under BinkDoFrame (`li r4,775; b sub_82AF8D38`).
- Kameo's older RAD build (2005): `sub_8270FF98`, under `sub_82709A20`.

The frame buffers are RAD's BINKFRAMEBUFFERS:

- +4 / +8 the Y plane's width and height, +12 / +16 the chroma planes', +20 FrameNum;
- then plane sets of 48 bytes from +24: Y, cR, cB, A, each {Allocate, Buffer, Pitch}.

It decodes from set FrameNum into FrameNum ^ 1 when that set has a Y buffer. The hook writes Y, then U into cB and V
into cR, marks the store cards, and returns 0.

**Two traps:**

- **The word swap.** By the time the video decode runs, Bink has byte-swapped the frame's packet in 32-bit words (its
  bit reader's order). In the file, `A5 5A C3 80 80 81 80 80` reads back as `80 C3 5A A5 80 80 81 80`, so FindTag
  reads through the swap. Blur's first two console runs were green movies; the PC found it with `blur_hevc_log`.
  All of Kameo's 119,503 frames and all of Blur's start their video data word-aligned.
- **FrameNum.** Kameo's decode stores FrameNum ^ 1 itself; Blur's leaves it to the caller. A hook that returns without
  decoding must do what the original does.

Results: 1.55 mean difference per channel against FFmpeg's Bink decode (Blur). Kameo at its 40 dB floor went from
2,631 to about 778 MB.

---

## 6. Audio to Opus

### What every audio path shares

- **Opus at 48 kHz or 24 kHz, 20 ms packets, VBR.**
  - Browser: WebCodecs `AudioEncoder`:
    ```js
    { codec: 'opus', sampleRate, numberOfChannels, bitrate, bitrateMode: 'variable',
      opus: { frameDuration: 20000, complexity: 10, application: 'audio', format: 'opus' } }
    ```
  - PC: `ffmpeg -c:a libopus -b:a RATE -vbr on -application audio -frame_duration 20 -compression_level 10`.
  - On the Switch: devkitPro's `switch-libopus` (1.3). The base links it through its `rexopus` target (port 259);
    the ports made before that link portlibs' `libopus.a` directly.
  - On a PC, `libopus-0.dll` is loaded at run time when present. Without it the PC plays the Opus sounds as silence,
    which every port saw: test the sound on the console.
- **Resampling.** Opus has no 44.1 or 22.05 kHz:
  - **`.xmop`, 44,100 Hz:** made at 48 kHz and resampled back by a 16-tap polyphase filter (160/147).
  - **`.xmop`, 22,050 Hz:** two ways, both 24 kHz Opus:
    - vivapinata-nx resamples back by the same 160/147 filter. That needs its one rate case in `xmo.cpp`
      (ParsePackSound: 22,050 Hz with 24 kHz Opus). **The base does not have it:** it accepts 48 / 48, 24 / 24 and
      44.1 / 48 only.
    - kameo-nx declares the sound 24,000 Hz in the `.xmop` (`lib/xwb.js` keeps the bank's rate apart), makes the
      24 kHz Opus from the same samples, and plays it unresampled. XMA frames are 512 samples at any rate and the
      game's voice keeps its own rate, so each frame is the samples it was. No SDK change is needed.
  - **`.opcm` and `.adxo`** always use 48 kHz Opus:
    - `.opcm` goes back to the PCM's rate through a 64-tap Kaiser-windowed sinc;
    - `.adxo` goes back to the stream's own rate (32 and 22.05 kHz too, SA2) through a zero-phase Kaiser sinc.
  - A 32 kHz-to-48 kHz-and-back round trip alone measured lag 0, 38 dB (the band above 15.5 kHz).
- **Seeking.** After a jump (a loop, a seek):
  - `.xmo` / `.xmop` and `.opcm` start decoding 320 ms before the target. Culdcept measured that 80 ms left the
    first frame 20-25 dB from the in-order decode, and 320 ms gave 92 dB or better over 52 seeks.
  - `.adxo` starts 4 packets (80 ms) before ("Opus converges in 80 ms", sa2-nx-ult `adx_opus.cpp`). By Culdcept's
    measurement, 320 ms is the safer choice there too.
- **How to check.** Opus is perceptual: its full-band waveform SNR against the original is 14-20 dB at any bitrate.
  That number is not the measure. Check instead that:
  - every byte outside the samples is as it was;
  - every tag is right;
  - the decode lines up at lag 0;
  - the port's decode matches a model of it (RR6: 52.5 dB against the tool's model; ffmpeg's newer libopus on x86
    against devkitPro's 1.3 on AArch64);
  - and, last, listen on the console.

**The bitrates the ports chose:**

| Game | Music | Voices | Effects / other |
|---|---|---|---|
| SASRT | 128 kbit/s stereo / 64 mono at 48 kHz (StreamsMulti) | 64 / 32 (the five language packages) | 24 kHz sounds 48 / 32; SoundBanks 96 / 64 |
| RR6 | 128 stereo (PCM resampled to 48 kHz) | | DJ and crowd clips stay XMA |
| Culdcept | 128 stereo | 44.1 kHz clips stay XMA | 96 stereo / 48 mono |
| SA2 | 128 stereo, mono 40 | | |
| NiGHTS | 128 | | |
| Viva | 44.1 kHz: 128 stereo, 64 mono | 22 kHz: 48 stereo, 32 mono | |
| Kameo | 96 stereo / 48 mono (24 kHz: 48 / 24); the user's choice over 128 / 64 | | |
| Blur | 128 stereo / 64 mono (`root_streaming.pak`) | | |
| Banjo | 128 (the PCM music) | | |

### Raw PCM to `NAME.opcm` (Ridge Racer 6, Banjo-Kazooie: Nuts & Bolts, Mushihimesama Futari)

**When:** the game reads its music as plain 16-bit PCM bytes. This is the biggest audio saving, about 90 %. Nothing in
the audio path changes: the file handle decodes the Opus and resamples it to the PCM's rate as the game reads, so the
game sees its PCM (lossy) at the same offsets.

The format, RXOPCM01 (`opus_pcm.h`):

- segments: stored bytes, or PCM;
- streams of 48 bytes: channels, 16 bits, the PCM rate, a flag for big-endian samples, the Opus pre-skip, frames,
  and the packet tables;
- 20 ms packets at 48 kHz.

| Game | Source | Result |
|---|---|---|
| RR6 | `pack_bgm.bin`: 20 RIFF WAVE tracks, 16-bit 44.1 kHz stereo, 765 MB | 71.8 MB |
| RR6 DLC | the 41 music packs, a WAV inside each Xbox 360 STFS package; one track is many ranges of the package, interleaved with its hash blocks | 2,040 to 196 MB. A package P is read through P.opcm when it lies beside it (`StfsContainerDevice::ReadAt`); P itself is then only the header the DLC scan reads |
| Banjo N&B | `Bundle/50/685374` (820 MB, inside a bundle, the first bank at byte 97,148, not aligned): 420 XACT 3 wave banks (version 43, `DNBW`, big-endian) | 819.5 to 357.9 MB: its 24 PCM sounds as Opus, the 15 music tracks about 4 MB each instead of about 41 |
| Futari | `media/Sound/BGM.xwb` (XACT 3 version 46: 13 sounds, PCM 16-bit big-endian 48 kHz stereo, 15.9 minutes, 174.3 MB) and `SYS_BGM.xwb` (14.7 MB) | about 15 + 1.2 MB (estimate). Already 48 kHz: no resampling |

Banjo's bank also holds 215 MB of XMA sounds. These could go to `.xmop`, but one file cannot be both `.opcm` and
`.xmop` without merging the two formats. Not done yet.

**Big-endian or little-endian?** Read neighbouring samples both ways. Read the right way, real audio steps a few hundred
(Banjo) to about 2,100 (Futari); read the wrong way, about 21,600-22,400.

**Cost:** opening `Bundle/50/685374.opcm` (819.5 MB, 420 banks) took 0.8-1.1 s in Ryujinx (Banjo), 0.48 s in a later
run. Worth trimming.

Tools:

- `tools/opus_pcm_pack.py` (rr6-nx): `--check` tests every stored byte, the SNR and the alignment; `--compare` checks
  an `opus_pcm_dump` of what the game read.
- banjo-nx-ult's `tools/xwb_opcm_pack.py` for XACT banks.
- Installer: `lib/opcm.js` (rr6-nx-installer), with banjo-nx-installer's XACT bank finder.

### XMA to `NAME.xmo` (one file) and `NAME.xmop` (many sounds in one file)

**When:** the game's sounds are XMA, decoded by the SDK's XMA emulation. The game keeps reading its own XMA bytes:

- the RIFF or container headers, every packet header, each frame's 15-bit length field and last bit stay as they were;
- each frame's other bits are zero, except a tag in each packet a frame starts in.

`XmaContext::DecodeXmoFrame` (`sdk/src/audio/xma_context.cpp`) reads the tag, finds the frame by its bit position, and
takes the frame's 512 samples from the sound's Opus. Loops, subframes and the output stay the emulation's own.

- **The tag, 72 bits:** 16 bits `0xA55C`, 32 bits the sound id, 24 bits the packet's index in the sound, MSB first, in
  the packet's tag bits (XmoTagBits):
  - the bits of the frames that start in the packet, after each one's length field;
  - then bits 47 onward, up to the packet's first frame (the end of a frame that began earlier).
- **Tag only the packets a frame starts in.** culdcept-nx 1.0.0 also tagged a sound's last packet when its skip count
  said a frame starts there but its first-frame offset lay past its end. That set the last bit of se_024's last frame,
  so the emulation read a frame at bit 90 whose length came from the tag (20,480 bits, past the file's end). Fixed in
  sasrt-nx's PacketMaker::Synthesize and culdcept-nx 1.0.1.

  culdcept's test had built its expected bytes by the reader's own rule. The 1.0.1 test reads every frame's length and
  last bit back from the synthesized file.
- **Keep a sound as XMA** when a packet a frame starts in has fewer than 72 tag bits (Viva: 24 sounds, Kameo: 29), or
  when its Opus is not smaller (Viva: 209 sounds, Kameo: 138 at 48 / 96).
- **Sound ids must be unique** among every `.xmo` and `.xmop` the game opens:
  - SASRT: `0x5A000000 | the package's place among the sorted .pck paths << 16 | the sound`;
  - Viva: `0x56000000 | bank << 12 | sound` (268 banks);
  - Kameo: its own top byte, `| bank << 12 | entry` (200 banks, at most 906 sounds a bank).
- **The Opus must come from the frames decoded in order** by the decoder the installer uses (FFmpeg's xmaframes: frame
  k is samples [512 k, 512 k + 512) after the pre-skip). `AnyXmoSound()` (base port 259) keeps a game without `.xmo` /
  `.xmop` files from reading packet tags.
- **XMA1 and XMA2** have the same packets (2048 bytes) and frames; only the fmt chunk differs. XMA1 is WAVEFORMATEX tag
  0x165, XMAWAVEFORMAT; XMA2 is tag 0x166, the 52-byte XMA2WAVEFORMATEX with block count, loop and bytes-per-block.
  Kameo's test sounds decoded **bit-identical** with FFmpeg's `xma1` from a 0x165 header and `xma2` from a 0x166 one.
  The packet header's first-frame offset and skip are where XMA2 has them, so the tag walk applies to both.
  culdcept-nx-installer's `lib/xmo.js` refuses anything but XMA1 (`'-3': 'not XMA1'`).
- **Cost on the console** (Culdcept): the XMA worker thread used 1.96 % of a core against 1.80 % with XMA. The worst
  context took 4.75 ms (a loop's 320 ms pre-roll, on the worker, not the game thread). A title music's data read in
  11 ms, against 52 ms as XMA.

**`.xmo`, RXXMAOP1 (culdcept-nx):** one RIFF XMA file a sound. The header holds the RIFF header, the packet headers, a
frame table (bit position, 15-bit length, last bit) and the trailer (the file's `seek` chunk), then the Opus packets.
**48 kHz only.**

**`.xmop`, RXXMAPK1 (sasrt-nx):** a package's bytes as segments (stored, zeros, a sound's synthesized packets) and a
sound table of 64 bytes a sound:

- the id, packets, frames, the XMA rate (24,000, 44,100 or 48,000), channels (1 or 2), the Opus pre-skip, packets and
  rate (48,000 or 24,000), the packet headers, the frame table and the Opus table;
- a 44,100 Hz sound uses 48 kHz Opus, and a 22,050 Hz sound 24 kHz Opus (vivapinata-nx).

It is container-agnostic: only the installer's parser knows the game's container.

**The containers met so far:**

| Game | Container | Sounds | Result |
|---|---|---|---|
| Culdcept | `data/SOUND/*.xma`: RIFF XMA1 files with a `seek` chunk, 48 kHz | music and effects; the 1,446 voice clips are 44.1 kHz and stay XMA (`.xmo` is 48 kHz only), as do 36 short effects smaller as XMA | 655 of 2,101 files, 290.1 to 178.5 MB (the page: 634 files to 171.2 MB) as `.xmo` |
| SASRT | Wwise AKPK packages (`Data/sound/*.pck`, big-endian RIFX WEMs, streamed or inside sound banks); XMA2 WEMs are single-stream, 2048-byte packets; a sound's packets are its WEM's data chunk | music XMA2 48 kHz stereo about 240 kbit/s; voices at 24, 44.1 and 48 kHz; SoundBanks' sounds 24 / 48 kHz, mono and stereo; SoundBanks' 98 MB of PCM stays | StreamsMulti 320.7 to 173.5 MB; SoundBanks 129.3 to 113.1 (1,168 of 1,170 XMA sounds); StreamsEng 80.6 to 31.5; the other four languages 187.6 to 77.6 MB |
| Ketsui | `Data/sd/MOD/*.xma`: 40 RIFF WAVE XMA2 files (fmt 0x166, one stereo stream, about 197 kbit/s, 62.8 MB) | the music | the 48 kHz tracks as `.xmo`, the 26 at 44.1 kHz as `.xmop` |
| Viva Piñata | XACT 2 wave banks (`DNBW`, versions 38-40; 268 banks, 133 streaming), all XMA, whole 2048-byte packets | 24,074 mono 22,050 Hz (voices in 17 languages, effects), 239 stereo 44.1 kHz (the music; one bank holds 117), 4 four-channel (21.4 MB; two interleaved streams: kept as XMA), and a small rest | 1,317.9 to 812.6 MB: 24,158 of 24,391 sounds as Opus |
| Kameo | XACT version 23 wave banks (`DNBW`; 200 banks, 46,810 sounds), all **XMA1** | 42,316 mono 44.1 kHz (113 kbit/s median), 187 stereo 44.1 kHz music (213 kbit/s), 4,205 mono 22.05 kHz, 68 stereo 22.05 kHz, 34 mono 48 kHz | 1,460 to 764 MB at 48 / 96 kbit/s (46,643 as Opus) |
| Blur | `.baf` banks stored inside the PAK2 file `cache/root_streaming.pak` (263 MB). A bank is a 4-byte word, then chunks of [4-byte tag][BE32 length]; a WAVE chunk of codec 25 is XMA (big-endian: +0x2C data offset, +0x30 size, +0x3C rate, +0x44 channels) | 187 sounds, each a single stream, 48 kHz, mono or stereo | as `.xmop` at 128 / 64 |
| Banjo N&B | XACT 3 version 43 (inside `Bundle/50/685374`) | 8,761 mono XMA (mostly 22 kHz, 70 MB), 170 stereo XMA (mostly 44.1 kHz, 138 MB), 1 four-channel | not done (the bank is `.opcm` for its PCM music) |

**The XACT wave bank** (big-endian, `DNBW`):

- **The header.** The version, then five segments (offset, length): bank data, entry meta, seek tables, names, wave
  data.
  - From byte 8 in versions 23 and 38-40 (Kameo, Viva).
  - From byte 12 in XACT 3 (versions 42-46: Banjo, Futari), which has a header version word first.
- **The bank data:** flags, the entry count, a 64-byte name, the entry meta size (24), the name size (64), the
  alignment (2048).
- **A 24-byte entry:** flags and duration, **the format word**, the play region (offset in the wave data, length), the
  loop region.
- **The format word is not the same in every version.** Read it big-endian, keeping the PC's bit order (low bits first):

  | Versions | Tag | Channels | Rate | Block align | Bits per sample |
  |---|---|---|---|---|---|
  | 38-46 (Viva, Banjo, Futari) | 2 bits: 0 PCM, 1 XMA, 2 ADPCM, 3 WMA (Futari's SE / VOICE banks) | 3 bits | 18 bits | 8 bits | 1 bit |
  | 23 (Kameo; the older MINIWAVEFORMAT) | 1 bit: 0 PCM, 1 XMA | 3 bits from bit 1 | 18 bits from bit 4 | 9 bits from bit 22 | 1 bit at bit 31 |

  Read with the newer layout, Kameo's words looked like "tag 3, 0 channels, 22,050 Hz". Read the older way, they give
  exactly the entry's duration (its first word >> 4) when decoded.
- **Confirm by decoding** one sound of each rate both ways, before trusting a reading.
- What looked like a wave bank in N&B's `Bundle/4f/3e5515` was `DNBW` bytes inside compressed data.

Tools:

- PC: sasrt-nx-installer's `node test/xmop_node.mjs IN OUT [--jobs N]` (ffmpeg's libopus) and `test/xmop_check.mjs`
  (every byte outside the sounds, every header, frame length, last bit and tag).
- culdcept-nx's `tools/test_xmo.cpp` and `tools/xmo_compare.py`.
- Installer: `lib/xmop.js` (the writer) with the game's parser:
  - `lib/wwise.js` (SASRT);
  - `lib/xact.js` (Viva; it reads only the newer format word, versions 38 and up);
  - `lib/xwb.js` with `lib/media.js` (Kameo: the version 23 reader, the bitrates and the keep rules);
  - `lib/pak2.js` (Blur);
  - `wasm/xma` (FFmpeg's xmaframes decoder, the port's own);
  - `lib/audio.js`, `lib/resample.js`.

### CRI ADX to `NAME.adxo` (Sonic Adventure 2, NiGHTS into Dreams)

**When:** the game decodes ADX in its own code (CRI's ADX library), so the XMA emulation never sees it. The game reads
its `.adx` or `.afs` with each 18-byte ADX block made a tag. A hook on the library's one decode step writes each tagged
block's 32 samples from the stream's Opus, resampled to the stream's rate, and ends the step at the first block that is
not a tag (an end marker), as the game's own step does.

The format, RXADXOP1 (sa2-nx-ult `adx_opus.h`):

- segments: stored bytes, zeros, or ADX blocks;
- a stream table: channels, the ADX rate, blocks a channel, the Opus packets.

The tag is 18 bytes:

- `7A D5 C3` (the scale field's high bit clear, so it is not an end marker);
- the stream id in 3 bytes and the block index in 3 bytes (7 bits each, high bit set);
- `0x80 | channel`, `0x80 | (the sum of those 7 bytes & 0x7F)`;
- 7 zero bytes.

**ADX itself:**

- **Encoding 3:** 4-bit ADPCM, 18-byte blocks (a 16-bit scale and 32 four-bit samples a channel; stereo interleaved
  L R), header versions 3 or 4, flags 0 (no encryption) in both games.
- **The decode:** scale = ((field ^ key) & 0x1FFF) + 1; sample = nibble x scale + ((c1 x s1 + c2 x s2) >> 12),
  clamped; high nibble first. A block whose scale has its high bit set ends the stream.
- **FFmpeg's `adpcm_adx` omits the + 1.** Its decode agrees only to 49.6 dB, so `tools/adx_opus_lib.mjs` decodes as
  the game does.

The hooks:

| Game | Library | Decode step | Mono / stereo decoders |
|---|---|---|---|
| SA2 | ADXT/XBOX360 11.35 | `sub_828C2948(obj)`: obj +12 the state, +16 blocks done, +20 channels, +24 the input, +28 blocks, +32 / +36 the outputs, +40 / +44 histories, +48 / +50 coefficients, +52 the key | `sub_828CCD28`; `sub_828CD3E0` (-> `sub_828CCE80` / `sub_828CD168`) |
| NiGHTS | ADXT 11.39, beside CRI Audio 3.57 | `sub_823BAB70`, instruction for instruction SA2's but for one global's address | `sub_823BBE58`, `sub_823BC510` |

The files and results:

- **SA2:** 157 `gd/ADX/*.adx` music files, 366 MB, 3 hours:
  - 134 stereo 32 kHz, 9 stereo 44.1 kHz, 13 mono 22.05 kHz, 1 header version 4; 103 loop;
  - in the browser they went from 365 to 164 MB (title.adx 0.75 to 0.33 MB).
- **NiGHTS:** 214 `afs/*.adx`, stereo, version 4 (210 at 48 kHz, 3 at 44.1, 1 at 11.025), 2.3 hours: 450 to 143 MB at
  128 kbit/s, every stream at lag 0.

**Not covered by the ADX hook:**

- **CRI AHX** (ADX signature, but encoding 0x10: an MPEG-1 Layer II variant): SA2's cutscene voices in
  `gd/event_adx.afs` and `event_adx_e.afs` (2 x 2,727 mono 22.05 kHz clips, about 63 kbit/s, 146 MB). Read the
  encoding byte, not only the signature. Opus for them (about half at 32 kbit/s) would need a hook on the AHX decoder.
- **CRI sound banks** `.csb` (`@UTF` tables, ADX inside AAX): SA2's 283 (7,467 sounds, 50 MB), NiGHTS's 22 (62 MB, CRI
  Audio's own decoder). Not converted; NiGHTS packs its `.csb` with ZXC (62 to 55 MB).

Another way was written up in sasrt-nx's handoff for NiGHTS ("way 1"): synthesize ADX blocks from the Opus as the
game reads them, with no game hook. It would be two lossy steps (Opus, then the ADX's ADPCM again). NiGHTS took SA2's
proven hook instead, which applied unchanged.

### What stays as it is

- **Movie soundtracks:** WMA 2 / WMA Pro inside ASF; ADX inside SFD and USM; Bink audio.
- **XACT WMA banks** (Futari's `SE.xwb`, `SYS_SE.xwb`, `VOICE.xwb`).
- **Small PCM sound effects.**
- **XMA sounds that are four channels in two streams, or with too few tag bits.**
- **Audio inside emulated hardware** (Guwange's arcade ROMs): it can only be packed.

---

## 7. The web installer's side

- **The pages** are culdcept-nx-installer's design, carried into every port's installer:
  - FFmpeg decoders in WebAssembly;
  - WebCodecs' H.265 and Opus encoders (the computer's video hardware);
  - pools of workers;
  - a `game.json` listing what to pack (`lz4`, `zxc`, `asis`, `skip`), the movies (`match`, `fraction`, `gop`,
    `zxc`) and the audio (`match`, `zxc_min_saving`, per-file `bitrates`);
  - a zip streamed as it is written.
- **The base's installer** (`D:\renut-ultimate\installer`) packs LZ4 only: `lib/lz4b.js`, `wasm/lz4.*` and the LZ4
  worker, with no `wasm/zxc`. The ZXC codec and the movie and audio writers live in the ports' installers (section 9).
- **Each worker takes its jobs one at a time.** Culdcept's first full run kept 34 movies as WMV and 63 sounds as XMA:
  the pool posted a busy worker its next job, and a second job reopened the one WebAssembly decoder under the first.
- **The page's output equals the PC tools'** byte for byte, given the same H.265 frames or Opus packets
  (`test/opcm_node.mjs`, `sfdh_node.mjs`, `movies_node.mjs`, `xmop_node.mjs`). Check a whole zip with
  `test/check_zip.py`.
- **Real browser runs measured:**
  - Culdcept, Chrome: 2,576 MB zip in 545 s.
  - RR6, headless Chrome: 358 s, music 831 to 77 MB, three movies 288 to 118 MB.
  - SA2, Chromium: 517 s, or 260 s without the interviews.
  - Banjo: 52 movies in 177 s on 2 workers; a whole install in 1,383 s, 723 s of it the shaders.
- **What the player gets:** an option to leave large unplayed files out, such as SA2's `"leave_out"` for its
  interviews. The re-encodes are boxes ticked by default.

---

## 8. Traps, all in one list

1. **A plain NAME beside its stand-in wins.** Move the original aside to test; an installer must not leave both.
2. **Never pack a linked game folder in place.** Banjo packed its master copy through an emulator SD link and had to
   restore 952 files. `tools/pack_game_root.py` now refuses a link: pack a copy, or hard-link the unchanged files into
   a test folder.
3. **The PC build may keep its saved paths** (`renut.cfg`) over `--game_data_root`. Kameo's first two test runs played
   the original folder because of it.
4. **A new `REX_HOOK_RAW` does nothing** until `tools/direct_calls.py --redo` / `tools/post_codegen.py` runs again.
   Bulk machine copies of a caller bypass it too.
5. **Raise the movie decode to 0x2C, and read ahead on a libnx thread** (not `std::thread`).
6. **Keep nxvideo's VIC copy and its clocks.**
7. **Ryujinx has no H.265 NVDEC.** Its "decoded" pictures are empty: test there with the software decoder, and the
   real thing on the console.
8. **Bink packets are word-swapped** when the video decode sees them. Kameo's decode flips FrameNum itself.
9. **SFD B pictures have their own planes**, not the reference's.
10. **ASF:** keep each audio object whole in one packet, and keep the original send times.
11. **VC-1:** other status codes in a later XDK build; the B-picture display rule; skipped 1-byte frames; the first
    picture only on drain.
12. **XMA tags only where a frame starts.** Test with a check that reads the frames back, not one that shares the
    reader's rule.
13. **The XACT format word differs by version.** Confirm by decoding.
14. **An `.adx` signature is not ADX:** read the encoding byte (AHX is 0x10).
15. **FFmpeg's ADX decode is not the game's** (the + 1 in the scale): decode as the game does, and make the Opus from
    that.
16. **The PC has no libopus** unless `libopus-0.dll` is beside it: silent Opus sounds there are expected.
17. **The Claude app's built-in browser refuses nested workers:** test the pages in Chrome or Edge.
18. **A full scan reads every stand-in's header** (tens of seconds on the console). With culdcept-nx's two-size VFS
    index, or the base's since port 264, only the first start after an install scans (section 3).
19. **Sizes:** say whether MB or MiB. SA2's "1,222 MB" zip was 1,166 MiB.

---

## 9. Where the code is, and what the base still lacks

**In the base:**

- **Port 61:** `lz4_blocks` version 1 (LZ4), and `tools/lz4b_pack.py` (LZ4 only).
- **Port 259:**
  - `lz4_blocks` version 2 (ZXC), `sfd_hevc`, `opus_pcm` / `opus_decoder`, `xmo` (`.xmo` and `.xmop`), `xmo_opus`,
    `XmaContext::DecodeXmoFrame`;
  - the merged lookup and VFS index version 2;
  - `rexopus`, `renut_nxvideo`;
  - `tools/zxcb_pack.cpp`, and rr6-nx's `tools/test_lz4_blocks.cpp` (both versions).
- **Port 264:** the VFS index's version 4, culdcept-nx's two sizes for every stand-in kind (added after this guide was
  written).

The movie hooks are each game's, and stay in the ports.

**Not in the base yet:**

- **`NAME.adxo`** (sa2-nx-ult's `sdk/include/rex/filesystem/adx_opus.h`, `sdk/src/filesystem/adx_opus.cpp`, and its
  lookup in `host_path_*`; nights-nx has the same). The hook is per game (`sa2_adx_opus.cpp`, `nights_adx_opus.cpp`).
  Two ports use it, so it is a candidate for the base.
- **The 22,050 Hz `.xmop` rate:** vivapinata-nx's case in `xmo.cpp` ParsePackSound (22,050 Hz with 24 kHz Opus). It
  is one line; Kameo's way needs none.
- **The ASF movie hooks' common part:**
  - `culdcept_wmv_ffmpeg.cpp` and its copies `banjo_wmv_ffmpeg.cpp` and `vivapinata_wmv_ffmpeg.cpp` differ only in
    addresses;
  - `nights_wmv_ffmpeg.cpp` is adapted for VC-1: other status codes, the B-picture rule, the first picture on drain,
    skipped frames.

  A candidate for a base helper with per-game addresses and status codes.
- **The installers' media writers** are in the ports' installers, not the base's: `lib/sfdh.js`, `usmh.js`,
  `biks.js`, `asf.js`, `movies.js`, `opcm.js`, `xmo.js`, `xmop.js`, `adx.js`, `wwise.js`, `xact.js`, `xwb.js`,
  `pak2.js`, `audio.js`, `resample.js`, and the wasm decoders.

**Where each piece lives** (read the working trees; some are uncommitted):

| Port | Folder | What to take from it |
|---|---|---|
| rr6-nx | `D:\ridge racer 6\rr6-nx`, `rr6-nx-installer` | the `.lz4b` reader (both versions), `.hevc` and `.opcm`, the lookup; `rr6_hevc_movies.cpp` (the SFD hooks, read-ahead, priority); `tools/hevc_sfd_pack.py`, `opus_pcm_pack.py`, `zxcb_pack.cpp`; installer `lib/sfdh.js`, `lib/opcm.js`, `wasm/src/mpeg1`; `docs/opus-h265-handoff-sasrt.md`, `-sa2.md` |
| culdcept-nx | `D:\culdceptsaga\culdcept-nx`, `culdcept-nx-installer` | `.xmo`, `DecodeXmoFrame`, `tools/switch/nxvideo`, `culdcept_wmv_ffmpeg.cpp` (the WMV9 hooks); installer `lib/asf.js`, `movies.js`, `xmo.js`, `audio.js`, `wasm/wmv3`; `docs/culdcept-notes.md` (the movie and Opus sections, the AV1 test) |
| sasrt-nx | `D:\sonic transformed\sasrt-nx`, `sasrt-nx-installer` | `.xmop` (`xmo.h`, `xmo.cpp`, PacketMaker), `sasrt_usm_ffmpeg.cpp` (USM, B-picture cache), `tools/hevc_usm_pack.py`; installer `lib/usmh.js`, `wwise.js`, `xmop.js`, `resample.js`, `test/xmop_node.mjs`, `xmop_check.mjs`, `pack_node.mjs`; `docs/media-compression-1121.md` |
| sa2-nx-ult | `D:\sonic adventure 2\sa2-nx-ult` (branch `ultimate`), `sa2-nx-ult-installer` (branch `sa2`) | `.adxo` (`adx_opus.h/.cpp`, `sa2_adx_opus.cpp`, `tools/adx_opus_pack.mjs`, `adx_opus_lib.mjs`), `sa2_hevc_movies.cpp`, `sa2_interviews.cpp` (XMedia2); installer `lib/adx.js`, `sfdh.js`, `movies.js`; `docs/opus-h265.md` |
| banjo-nx | `D:\Banjo kazooie port\banjo-nx-ult`, `banjo-nx-installer` (branch `media-pack`) | `banjo_wmv_ffmpeg.cpp`, `tools/xwb_opcm_pack.py`, `hevc_movie.py`, `pack_game_root.py`; the XACT finder; `docs/media-compression.md` |
| vivapinata-nx | `D:\viva pinata\vivapinata-nx`, `vivapinata-nx-installer` | `vivapinata_wmv_ffmpeg.cpp`, XACT 2 `.xmop`, 24 kHz Opus; installer `lib/xact.js`, `test/xact_survey.mjs`, `lz4_measure.mjs`; `docs/media.md` |
| kameo-nx | `D:\kameo\kameo-nx`, `kameo-nx-installer` | `kameo_bink_hevc.cpp` (older Bink), `tools/bik_hevc_pack.py` with the PSNR-floor options; installer `lib/xwb.js` (the XACT v23 format word, 22 kHz as 24 kHz), `lib/media.js`, `lib/movies.js`; `docs/kameo-media-plan.md` |
| blur-nx | `D:\blur\blur-nx`, `blur-nx-installer` | `blur_bink_hevc.cpp` (Bink, the word swap), `tools/bik_hevc_pack.py`; installer `lib/biks.js`, `pak2.js`, `xmop.js` (`.baf`), `wasm/bink` |
| nights-nx | `D:\nights\nights-nx`, `nights-nx-installer` | `nights_wmv_ffmpeg.cpp` (VC-1), `nights_adx_opus.cpp`; installer `wasm/vc1`; `docs/nights-notes.md` "Packed files, H.265 movies and Opus music" |
| futari-nx, ketsui-nx, guwange-nx | `D:\mushihemesama\*` | `.opcm` for XACT PCM (Futari), `.xmo` / `.xmop` for RIFF XMA2 (Ketsui), packing only (Guwange) |

---

## 10. Credits

- **culdcept-nx:** the method (tagged XMA frames, `.xmo`, WMV3 to H.265 in the ASF, nxvideo on NVDEC, the VIC copy,
  the clocks, the decode priority).
- **rr6-nx:** the `.lz4b` container and its ZXC version, the `.hevc` and `.opcm` formats, the lookup order, the
  read-ahead.
- **sasrt-nx:** `.xmop`, the USM path, the tag-where-a-frame-starts fix.
- **sa2-nx-ult:** `.adxo`.
- **blur-nx:** Bink 1.
- **banjo-nx and vivapinata-nx:** XACT and Rare's formats.
- **kameo-nx:** the XACT v23 format word, the quality floor.
- **nights-nx:** VC-1.
- **tw2de_nx and sasrt-nx:** the method of putting FFmpeg in place of a game's own movie decoder.
- **masseffect-nx:** the SDK FFmpeg's WMV3 / VC-1 configuration.

**Licences:**

- LZ4 is BSD-2.
- ZXC: see `sdk/thirdparty/zxc/LICENSE`.
- libopus is BSD-3.
- FFmpeg as built with nvtegra is GPL.
- THIRD_PARTY_NOTICES.md in rr6-nx and sasrt-nx has the rows.

katamari-nx: Beautiful Katamari (Xbox 360, title 4E4D07DC) on the Nintendo Switch
==================================================================================

A ReXGlue static recompilation, built from banjo-nx (Banjo-Kazooie: Nuts & Bolts' port): its SDK fork, its
post-codegen CPU passes and reNut's Direct3D-level renderer on plume's deko3d back end (no NVK, no PM4 ring, no EDRAM
model). Your own game files are needed; nothing of the game is in this package except the recompiled code.

Credits: reNut's renderer by masterspike52 (its author permits its reuse in these ports), banjo-nx, sa2-nx (input,
CPU items), NetherSX2_nx by NaGaa95 (the four-core layout, MIT), battd_nx (the GPU arena), bejeweled3-nx and the zuma
ports (the hot-function method). GPL-3.0 where the sources are.

SD card layout
--------------
sdmc:/switch/katamari-nx/katamari-nx.nro      the program
sdmc:/switch/katamari-nx/renut.toml           settings (copy one of the tomls below over it)
sdmc:/switch/katamari-nx/game_root/           the disc's files: default.xex, d02.dat-d05.dat, data/, alchemy.ini ...
                                              (extract your ISO; the ISO itself is not read)
sdmc:/switch/katamari-nx/dlc/                 the DLC packages (Xbox 360 LIVE files, content type 2), installed at the
                                              first start into sdmc:/switch/katamari-nx/renut/ (dlc/installed.txt lists
                                              them); all 16 of Beautiful Katamari's packs load

The tomls
---------
renut.toml        the shipped settings: Renut on deko3d, 720 lines handheld and 1080 docked (your choice; the
                  game's own MSAA, no dynamic resolution), every exact enhancement on, the items measured SLOWER ON
                  THE CONSOLE in banjo-nx off, logs and diagnostics on. configs/renut_720_docked.toml keeps the Xbox
                  360's own 720 lines docked too (pixel-identical to the console).
renut_play.toml   the same with the render-thread diagnostics off (for playing).
renut_ab.toml     the same with an in-run A/B list, 30 s halves, target 1 as the do-nothing control (a Vulkan-only
                  key). Stand still in one place for 10+ minutes; the log's "[renut A/B]" lines give each target.
configs/          options that change the picture, one key away from renut.toml each, for you to decide on.

Cores
-----
The port runs on the three cores a homebrew launch gets. Four cores are optional and off: with
banjo_forwarder_program_id in renut.toml set to your forwarder's id, the NRO writes that forwarder's four-core NPDM
(NetherSX2_nx's layout, CPU 0-3) to sdmc:/atmosphere/contents/<id>/exefs/main.npdm, and the next launch has CPU 0-3
(the log's "[banjo] system core:" line says whether the fourth core was granted). Delete that main.npdm to go back to
three cores. (Packages made with -FourCore also carry it under COPY_TO_SD_ROOT for program id 05D6EA1333487100.)

Profiling
---------
The ELF (katamari-nx.elf) and the source snapshot (source/) are the ones this NRO was built from: a console profile
(rex_profile.log, switch_profile_stacks) maps to source lines with them.

The CPU enhancements of this port (each with its key in renut.toml; all exact, all on)
--------------------------------------------------------------------------------------
katamari_hand_matrix_copy         hand-written native of the game's 4x4 matrix copy (the hottest guest function)
katamari_hand_matrix_multiply     hand-written native of its two 4x4 matrix multiplies (in place, three operands)
katamari_hand_matrix_translation  hand-written native of its translation-matrix builder
banjo_hand_memset                 the C runtime memset (banjo-nx's native: the same routine)
banjo_cpu_native_sub_*            the localized copies of the 100 hottest other functions (PC profile)
In-run A/B: target 69 = the hand-written matrix natives, 70 = the localized copies (renut_ab.toml lists both).
A guard compares a native with the generated original now and then (banjo_hot_guard_calls / _period); a difference
switches that one native off and logs "[hot] DIFFERENCE".

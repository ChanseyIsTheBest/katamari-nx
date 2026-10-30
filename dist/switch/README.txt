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

The tomls
---------
renut.toml        the shipped settings: Renut on deko3d, pixel-identical to the console (720 lines handheld and
                  docked, the game's own MSAA, no dynamic resolution), every exact enhancement on, the items measured
                  SLOWER ON THE CONSOLE in banjo-nx off, logs and diagnostics on.
renut_play.toml   the same with the render-thread diagnostics off (for playing).
renut_ab.toml     the same with an in-run A/B list, 30 s halves, target 1 as the do-nothing control (a Vulkan-only
                  key). Stand still in one place for 10+ minutes; the log's "[renut A/B]" lines give each target.
configs/          options that change the picture, one key away from renut.toml each, for you to decide on.

Four cores
----------
COPY_TO_SD_ROOT/atmosphere/contents/05D6EA1333487100/exefs/main.npdm is NetherSX2_nx's forwarder NPDM layout (CPU
0-3) with program id 05D6EA1333487100. Make a forwarder with that program id for the NRO (or change
banjo_forwarder_program_id in renut.toml to your forwarder's id and let the NRO write the NPDM itself: launch once,
then again). The log's "[banjo] system core:" line says whether the fourth core was granted.

Profiling
---------
The ELF (katamari-nx.elf) and the source snapshot (source/) are the ones this NRO was built from: a console profile
(rex_profile.log, switch_profile_stacks) maps to source lines with them.

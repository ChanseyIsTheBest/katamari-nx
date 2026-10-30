# katamari-nx progress log (2026-10-30, AEST)

| Time | What |
|---|---|
| 10:42 | Start. D: 10.3 GB free, C: 39 GB. Read banjo-nx's porting-start-here, the renderer guide, the enhancements reference's top table, the XDK versions doc. |
| 10:45 | ISO listed (2.68 GB of files) and extracted to `D:\beautifulkatamari\game`. XEX: title 4E4D07DC, XDK 2.0.5632. |
| 10:48 | katamari-nx made from banjo-nx's SDK, app, tools, shaders (robocopy); Banjo's address lists emptied. |
| 10:55 | Codegen 1-5: three missing function starts, two split `li r3,0; blr` tails; clean. git init, first commit. |
| 11:00 | Pointer sweep to its fixpoint: 111 functions reachable only by pointer. |
| 11:05 | App trimmed for a first boot with the SDK's emulated GPU (Renut's hooks fenced, Banjo-address files out). |
| 11:10 | First PC build started (`tools/build_pc.bat`, `app/out/pc`, Renut OFF). sasrt-nx's note on Renut/plume fixes: plume's already here; resolve readback to check once Renut runs. |
| 11:15-11:50 | Renut's hooks moved: 222 state functions from the state tables (Katamari's at 0x82AD8E48 / 0x82AD9308), 60 by signature, 40 by hand; device layout zones and data addresses derived from 288 paired functions; 165 offset lines rewritten; tiling adapters for XDK 5632's full BeginTiling / EndTiling. Three build restarts (my own file read during a pass, BOMs from PowerShell, a missing `generated/hot`). |
| 11:55 | PC build linked (emulated GPU, Renut out). Boot 1-2: no log (the buffered writer, then banjo-nx's path wizard waiting for a click: `renut.cfg` written). |
| 12:00 | Boot 3: the runtime starts, mounts the game, reads `alchemy.ini`, stops on an unregistered function 0x82689770 (a leaf in a gap). Gap sweep (`tools/gap_sweep.py`): 522 complete functions in gaps. |

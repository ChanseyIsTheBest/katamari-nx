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

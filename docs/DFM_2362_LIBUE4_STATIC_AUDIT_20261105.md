# DFM 2362 user-supplied libUE4.so: offline audit (2026-11-05)

- Input: `libUE4.zip` (single `libUE4.so` entry; ZIP CRC passed)
- ELF: stripped AArch64 ELF64 shared object, 561,598,448 bytes
- Build ID: `8cbb29a1d7283485daf2afc5f257fefb76e96b13`
- SHA256 of extracted SO: `686577e4acf4bdd45b113b579d72dd581e9beffb6f6d04d07871b0341ff470c1`
- Previous 2207 Build ID (handoff record): `2cdcd1a90870be747f3090993b73383359c17f47`
- Symbol status: no `.symtab` or `.debug_info`; only 935 entries in `.dynsym`
- Spot-check: selected UE class names (e.g. `UWorld`, `USceneComponent`, `FNamePool`, `FEncVector`) not present as direct ASCII/UTF16LE literals; does not prove their code is absent.

## Existing 2362 runtime evidence supplied by user

- NamePool candidate RVA: `0x200F7D00`, first name `None` was decoded by diagnostic.
- GUObjectArray candidate RVA: `0x2011EB48`, object count 238,803, and 24/24 index checks matched.
- ELF's third loaded segment has file-backed data ending at RVA `0x1F5F3BCC`, with zero-filled runtime memory through `0x204C0B38`. **Both candidate RVAs fall in the zero-filled runtime interval**, so the file does not contain the instantiated name pool/object array.
- The candidate RVAs are not yet fully validated by an SDK export, so do not put them into production configuration without verification.

## Status and next step

- Static integrity and ELF metadata: COMPLETE.
- New 2362 AIOHeader.hpp SDK: NOT AVAILABLE from this stripped SO alone.
- Runtime global candidates: PROVISIONAL, not stable validated offsets.
- Actual SDK class/member layout migration: BLOCKED pending matching authorized developer reflection export / SDK declarations / debug symbols.
- With the version-matched SDK, run the existing `scripts/sdk_layout_diff.py` to compare classes and fields against the 2207 archive.
- The supplied ELF has not been independently matched to the installed APK's ELF SHA256; user provenance only.

This report does not include a runtime memory reader, an exploit or anti-cheat bypass. The previous 2207 baseline and `dfm_gm` are unchanged.

# DFM 2362 / SDK baseline audit (2026-11-05)

## Verified old SDK from Library
- Source: `dump sdk.zip`, 6,344,764 bytes; archive CRC pass.
- ZIP SHA256: `12bf5fb65aafcedf9611153d04f214e3b8588886284e7932afbc60c57a06b901`.
- Contains: `AIOHeader.hpp`, `Offsets.hpp`, `Objects.txt`, `Logs.txt`, `script.json`.
- Old 2207 Logs: 114,477 UObject; 12,712 classes; 8,672 structs; 3,572 enums; 633 packages.
- Offline parser review: 21,383 class/struct blocks, 129,693 non-padding member declarations; 17,797 bitfields. These are parser-recognized counts, not independent proof of runtime accuracy.
- Caveat: some initial name entries in the old dump's `Logs.txt` are garbled.

## 2362 diagnostics (candidates only)
- New APK: 1.201.37119.49 (versionCode 2362), arm64.
- Candidate NamePool RVA `0x200F7D00` decoded initial `None`.
- Candidate GUObjectArray RVA `0x2011EB48`, 238,803 objects reported and 24/24 object-index structure checks.
- Old signature scans: 0/6 matched.
- The old `--qa-dfm-dump` stops with `ERROR_INIT_NAMEPOOL`.
- **These are not fully verified SDK globals or user-class layout offsets.** Do not auto-deploy candidates.

## Offline comparison readiness
- Updated `scripts/sdk_layout_diff.py`: now reads bitfield width and mask, in addition to member type, size and byte offset; excludes padding.
- Added `scripts/test_sdk_layout_diff.py`: isolated fixture-based tests.
- The source file updates are committed; the repository's test script has **not** been run in this environment. An independent regex parser was run against the old SDK archive.
- No 2362 `AIOHeader.hpp` / authenticated symbol package was found in the inspected Library/GitHub materials. Actual *two-version* field differences are **not yet computed**.

When an authorized 2362 SDK or debug-symbol-derived declaration is available:
```bash
python3 scripts/sdk_layout_diff.py \
  --old /path/to/2207/AIOHeader.hpp \
  --new /path/to/2362/AIOHeader.hpp \
  --csv sdk_diff_2207_2362.csv --json sdk_diff_2207_2362.json
```
This script is offline-only. Preserve the working 2207 branch and do not modify the live client configuration without validation.

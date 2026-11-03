# AGENTS.md

Rules for agents working on this tsschecker fork. `CLAUDE.md` is the primary guide (mission, architecture,
current state, validation); this file repeats what is needed to work safely.

## What this is
A C++17 CLI and library that sends TSS requests to `gs.apple.com` to check firmware signing and save `.shsh2`
blobs. `main.cpp` (CLI) → `TssRequest.cpp` (request builder and transport) → `FirmwareAPI*.cpp` (ipsw.me v4
and AppleDB firmware lists) → `tsschecker.cpp` (helpers, libirecovery lookups, baseband table).

## Rules
- TSS requests must match libtatsu (`libimobiledevice/libtatsu`, `src/tss.c`). Check the libtatsu function before
  adding, removing or changing a request key, and run the parity script after:
  `python3 .claude/skills/tss-parity/scripts/compare_ap.py BuildManifest.plist req.txt`.
- Request content belongs in `TssRequest`, not in `main.cpp`. Device facts libirecovery lacks go in `bbdevices[]`.
- Use the libgeneral error macros (`retassure`, `retcustomassure`, `retassureMissingValue`, `cleanup`).
- Test with a made-up ECID (`-e 0x1210ce2208802e`), never the user's real device values unless asked.
- Don't push, force-push, or open PRs without being asked. `CLAUDE.md`, `AGENTS.md` and `.claude/` are fork-only.
- Don't commit `MEMORY.md` or `.claude/` (both are excluded in `.git/info/exclude`).

## Validation
```sh
git submodule update --init
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./autogen.sh && make -j$(nproc)
LD_LIBRARY_PATH=/usr/local/lib ./tsschecker/tsschecker -d iPhone12,5 -l -e 0x1210ce2208802e
```
Expect `Status: 0 (SUCCESS)`. There is no test suite; a change is validated by a clean build plus live requests
(restore, OTA with `-o`, and an A18 device such as iPhone17,1).

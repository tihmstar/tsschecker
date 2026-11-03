# tsschecker Claude Instructions

## Mission
tsschecker asks Apple's TSS server (`gs.apple.com`) whether a firmware is signed for a device and can save the
returned tickets (`.shsh2` blobs) for later restores. This is Cole's fork (`origin` = coleleavitt/tsschecker,
`upstream` = tihmstar/tsschecker), branch `fix-tss-ios27-status94`.

What must be preserved:
- **Requests must match what libtatsu sends** (libimobiledevice/libtatsu `src/tss.c`, used by idevicerestore).
  TSS rejects requests that differ (status 69, 94, 133 and 161 all came from differences), and a ticket built from
  a different request may not match what a restore asks for.
- A saved blob has to be usable later, so the request fields that end up in the ticket matter even when TSS returns
  status 0 without them.

## Architecture First
Layers, top to bottom. Each layer only calls the ones below it.

| layer | files | owns |
|---|---|---|
| CLI | `tsschecker/main.cpp` | option parsing, device resolution, choosing firmware, printing, saving blobs |
| request builder + transport | `tsschecker/TssRequest.cpp`, `include/tsschecker/TssRequest.hpp` | build the TSS plist from a BuildManifest identity, send it, parse the reply |
| firmware lists | `tsschecker/FirmwareAPI.cpp` (selection, cache dir), `tsschecker/FirmwareAPI_IPSWME.cpp` (ipsw.me v4 API) | which IPSW/OTA exists for a device and its URL |
| helpers + device database | `tsschecker/tsschecker.cpp` | parsing, downloads (curl, libfragmentzip), libirecovery lookups, baseband table `bbdevices[]`, nonce type per CPID, plist iteration |
| errors | `tsschecker/TSSException.cpp`, `include/tsschecker/TSSException.hpp` | exception types the CLI catches |

External libraries: libgeneral (`retassure`/`cleanup`/`info` macros, `Mem`, `readFile`/`writeFile`), libplist
(>= 2.3), libirecovery (device database), libfragmentzip (reads `BuildManifest.plist` out of a remote zip), curl,
openssl, and the `external/jssy` submodule (JSON tokenizer).

Rules for where code goes:
- Request content (which keys, which components, restore rules) lives in `TssRequest`. `main.cpp` only calls the
  `add*`/`set*` methods in order: `setDeviceVals` → `setEcid` → `addDefaultAPTagsToRequest` →
  `addAllAPComponentsToRequest` → nonces → baseband. `addAllAPComponentsToRequest` reads `@ApImg4Ticket`, so the
  AP tags must come first.
- Device facts that libirecovery does not have (baseband gold cert id and SNUM length) live in `bbdevices[]` in
  `tsschecker.cpp`, synced with 1Conan/tsschecker.
- Firmware list sources implement `FirmwareAPI`. Version/build selection is in the base class
  (`FirmwareAPI::getURLForDeviceAndBuild`); a source only lists firmwares.

## Current State
- Status 94 fix (`ccc1beb`): IMG4 requests only carry components that have `RestoreRequestRules` or are `Trusted`
  (Savage, Yonkers, BMU, eUICC, SE made TSS answer 94). Also PSI/PSI2 choice by `BbGoldCertId` (status 133),
  `BbChipID` as integer, full 64-bit ECID, `Ap,*` identity fields, `libauthinstall-1104.0.9`, `@HostPlatformInfo=mac`.
- Audit follow-up (`f8e854b`): `Ap,SikaFuse=0` with `UID_MODE` for `RequiresUIDMode` devices (A18+, status 69
  without it), NeRD/Pearl fields, `@BBTicket`, baseband keys removed when the baseband part fails (status 161),
  updated baseband table, libtatsu restore-rule semantics, no NULL product type crashes, CLI fixes
  (`-s <path>`, `-v`, `--bbsnum`), cache in `$XDG_CACHE_HOME`/`~/.cache/tsschecker/`.
- Firmware lists use the ipsw.me v4 API (`/v4/devices`, `/v4/device/<id>?type=ipsw|ota`). The v2.1
  `ota.json` endpoint returns `null`. OTA lists skip betas unless `--beta`, prefer full OTAs over deltas, and drop
  the `9.9.` version prefix of full OTAs.
- Verified live: iPhone12,5 (27.0.1, 27.2, OTA 27.2) and iPhone17,1 (27.2) return status 0 with `ApImg4Ticket` and
  `BBTicket`; AP components match libtatsu (`.claude/skills/tss-parity`).
- Cryptex1 ticket (`--cryptex-nonce` + `--cryptex-seed`): separate request built by `TssRequest::makeCryptex1Request`
  (libtatsu `tss_request_add_cryptex_tags` plus the device fields idevicerestore gets from DeviceGeneratedRequest).
  TSS answers 168 without `Cryptex1,ProductionMode` or `Cryptex1,UDID` (BE64 ChipID + BE64 ECID). The Cryptex1 manifest
  components must be sent, or the ticket holds no image digests (the justtestingafewthings/1Conan C fork's filter drops
  them). Saved as `cryptexTicket`/`cryptexSeed`. When saving an Erase ticket without `-u`/`-V`, an `updateInstall`
  ticket is saved too (futurerestore `-u`).
- Known gaps: Yonkers/Savage/SE/Rose tickets (separate requests in libtatsu) are not implemented
  (`addYonkersComponentsToRequest` is a stub). iOS 4 without APTickets is a TODO. ipsw.me's `/v4/devices` has no
  Watch6+ entries (they have no IPSWs), so `--list-devices` doesn't show them, and it has no OTA entries for some new devices (iPhone17,1 gives "no firmwares found" with `-o`). SSL verification is off for TSS, as in
  libtatsu.

## Working Rules
- C++17, clang. Use the libgeneral macros already used in the file: `retassure`, `retcustomassure`,
  `retassureMissingValue` (baseband code relies on `TSSException_missingValue` key names), `cleanup`, `safeFree`.
- Compare against libtatsu `src/tss.c` before changing request content, and cite the function in a comment when a
  rule comes from there.
- Don't add fields "just in case": every request key must come from libtatsu or a verified TSS reply.
- Never send real device identifiers from the user's devices unless asked. Use `-e 0x1210ce2208802e` or a random
  ECID for tests.
- Keep commits small. Don't push or open upstream PRs without being asked. This file, `AGENTS.md` and `.claude/` are
  fork-only and should be dropped from an upstream PR.

## Validation
Build (the libs live in `/usr/local`):
```sh
git submodule update --init                     # external/jssy
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig ./autogen.sh
make -j$(nproc)
```
Run the built binary (libtool wrapper):
```sh
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./tsschecker/tsschecker -d iPhone12,5 -l -e 0x1210ce2208802e            # restore, latest
./tsschecker/tsschecker -d iPhone17,1 -l -e 0x1210ce2208802e -s .         # A18: SikaFuse + baseband
./tsschecker/tsschecker -o -d iPhone12,5 -l -e 0x1210ce2208802e          # OTA
./tsschecker/tsschecker -d iPhone12,5 -m BuildManifest.plist --print-tss-request > req.txt
python3 .claude/skills/tss-parity/scripts/compare_ap.py BuildManifest.plist req.txt
```
Expected: `Status: 0 (SUCCESS)` and `IS signed!`; with `-s`, the `.shsh2` contains `ApImg4Ticket` and `BBTicket`.
Status codes: 69/94 not eligible (often a request difference), 133 baseband PSI mismatch, 161 malformed request.
There are no unit tests; validation is a build plus live TSS requests.

## Project Claude Layout
- `AGENTS.md`: the same rules for agents that don't read this file. Keep the two consistent.
- `MEMORY.md`: local state and handoff notes (not committed).
- `.claude/` (not committed): `rules/` (code style, security, testing), `agents/` (researcher, verifier),
  `commands/` (`/audit`, `/repro`), `skills/tss-parity/` (libtatsu comparison script).

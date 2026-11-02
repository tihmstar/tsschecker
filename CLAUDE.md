# tsschecker - Agent Notes

## iOS 27+ Status 94 Fix (November 2026)

### Problem
tsschecker returned Status 94 ("device not eligible") for ALL devices on iOS 27.x, even though firmware was signed.

### Root Cause
The TSS request included `Savage,*`, `Yonkers,*`, `BMU,*`, `eUICC,*`, and `SE,*` components for IMG4 requests. Apple's TSS server rejects requests containing Savage components.

`libtatsu` (used by `idevicerestore`) only includes components that have `RestoreRequestRules` or are `Trusted`. tsschecker was including everything.

### The Fix (PR #180)
Key change in `TssRequest.cpp`:
```cpp
// IMG4: only components with RestoreRequestRules or Trusted are requested
// Others (Savage, Yonkers, eUICC, ...) make TSS reply with status 94
if (isImg4 && !(pInfo && plist_dict_get_item(pInfo, "RestoreRequestRules")) && !isTrusted) {
    return NULL;  // Skip this component
}
```

### Additional Fixes
- **Baseband PSI/PSI2**: Select based on `BbGoldCertId` (fixes Status 133)
- **ECID truncation**: Was `(uint32_t)ret`, now full `uint64_t`
- **BbChipID format**: Send as integer, not hex string
- **Path handling**: Was appending `'\n'` instead of `'/'`
- **Ap,* fields**: Added missing identity fields
- **Version string**: Updated to `libauthinstall-1104.0.9`
- **HostPlatformInfo**: "mac" on Linux (matches libtatsu)

### Follow-up fixes (audit against libtatsu)
- `Ap,SikaFuse=0` with `UID_MODE` for `RequiresUIDMode` devices (A18+): without it TSS answers Status 69
- `NeRDEpoch`, `PearlCertificationRootPub`, `AllowNeRDBoot`, `PermitNeRDPivot` copied like libtatsu (ticket gets nrde/pcrp/pndp)
- `@BBTicket` requested, so the response contains a BBTicket; `BasebandFirmware` sent without `Info`
- Baseband keys removed when the baseband part can't be built (partial baseband request = Status 161)
- Baseband table synced with 1Conan/tsschecker (iPhone 12-17, iPads, Watches, Macs)
- Restore rules: unknown conditions warn and fail the rule instead of aborting; only bool actions applied
- Components without RestoreRequestRules get EPRO/ESEC; components without Info are skipped
- ProductType stored in the request; unknown devices give errors instead of segfaults
- `-s <path>`, `-v`, `--bbsnum` work; cache in `$XDG_CACHE_HOME`/`~/.cache/tsschecker/`

### Known limitation
`-o` (OTA) is broken: `https://api.ipsw.me/v2.1/ota.json/condensed` answers `null`. Use `-m` with an OTA BuildManifest.

### Testing
```bash
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
./tsschecker -d iPhone12,5 -B d431ap -e 0x001210ce2208802e -l -s ~/shsh_blobs/
./tsschecker -d iPhone17,1 -e 0x001210ce2208802e -l -s ~/shsh_blobs/
# Status: 0 (SUCCESS), response has ApImg4Ticket and BBTicket
```

### Build Dependencies
- libgeneral
- libfragmentzip  
- libirecovery
- libplist (>= 2.3)
- curl
- openssl

### Build
```bash
git submodule update --init   # external/jssy
./autogen.sh
./configure
make -j$(nproc)
sudo make install
```

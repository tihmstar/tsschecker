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

### Testing
```bash
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
./tsschecker -d iPhone12,5 -B d431ap -e 0x001210ce2208802e -l -s ~/shsh_blobs/
# Status: 0 (SUCCESS) ✓
```

### Build Dependencies
- libgeneral
- libfragmentzip  
- libirecovery
- libplist
- curl
- openssl

### Build
```bash
./autogen.sh
./configure
make -j$(nproc)
sudo make install
```

Note: May need `#include <algorithm>` in `FirmwareAPI_IPSWME.cpp` for `std::min`.

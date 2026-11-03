//
//  FirmwareAPI_AppleDB.hpp
//  tsschecker
//

#ifndef FirmwareAPI_AppleDB_hpp
#define FirmwareAPI_AppleDB_hpp

#include <tsschecker/FirmwareAPI_JSON.hpp>
#include <plist/plist.h>
#include <map>
#include <string>
#include <vector>

namespace tihmstar{
namespace tsschecker {
//firmware lists from AppleDB (https://api.appledb.dev/).
//ios/<OS>/main.json lists every build with its devices and IPSW links. OTA links are only in the
//per-build record ios/<key>.json, which is fetched for the build that gets selected.
class FirmwareAPI_AppleDB : public FirmwareAPI_JSON {
    bool _ota;
    bool _beta;

    std::vector<std::string> _devicesCache;
    std::map<std::string,std::vector<firmwareVersion>> _versionsCache;
    std::map<std::string,plist_t> _docs; //URL -> parsed response

    plist_t fetchPlist(const std::string &url, plist_type type);
protected:
    virtual bool resolveFirmwareURL(firmwareVersion &f, const std::string &device) override;
public:
    FirmwareAPI_AppleDB(bool ota = false, bool beta = false);
    virtual ~FirmwareAPI_AppleDB() override;

    virtual void load() override;

    virtual std::vector<std::string> listDevices() override;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) override;
};
};
};

#endif /* FirmwareAPI_AppleDB_hpp */

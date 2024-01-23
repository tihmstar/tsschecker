//
//  FirmwareAPI_IPSWME.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef FirmwareAPI_IPSWME_hpp
#define FirmwareAPI_IPSWME_hpp

#include <tsschecker/FirmwareAPI.hpp>
#include <libgeneral/Mem.hpp>
#include <stdint.h>
#include <vector>
#include <map>

struct jssytok;
namespace tihmstar{
namespace tsschecker {
class FirmwareAPI_IPSWME : public FirmwareAPI {
    bool _ota;
    jssytok *_tokens;

    tihmstar::Mem _buf;
    std::vector<std::string> _devicesCache;
    std::map<std::string,std::vector<firmwareVersion>> _versionsCache;
public:
    FirmwareAPI_IPSWME(bool ota = false);
    virtual ~FirmwareAPI_IPSWME() override;
    
    virtual void load() override;
    virtual void loadcache() override;
    virtual void storecache() override;
    
    virtual std::vector<std::string> listDevices() override;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) override;
    virtual firmwareVersion getURLForDeviceAndBuild(uint32_t cpid, uint32_t bdid, std::string version = "", std::string build = "") override;
};
};
};

#endif /* FirmwareAPI_IPSWME_hpp */

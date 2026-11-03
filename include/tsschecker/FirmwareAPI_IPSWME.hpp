//
//  FirmwareAPI_IPSWME.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef FirmwareAPI_IPSWME_hpp
#define FirmwareAPI_IPSWME_hpp

#include <tsschecker/FirmwareAPI_JSON.hpp>
#include <stdint.h>
#include <vector>
#include <map>
#include <string>

namespace tihmstar{
namespace tsschecker {
//firmware lists from the ipsw.me v4 API (https://api.ipsw.me/v4/)
class FirmwareAPI_IPSWME : public FirmwareAPI_JSON {
    bool _ota;
    bool _beta;

    std::vector<std::string> _devicesCache;
    std::map<std::string,std::vector<firmwareVersion>> _versionsCache;
public:
    FirmwareAPI_IPSWME(bool ota = false, bool beta = false);
    virtual ~FirmwareAPI_IPSWME() override;
    
    virtual void load() override;
    
    virtual std::vector<std::string> listDevices() override;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) override;
};
};
};

#endif /* FirmwareAPI_IPSWME_hpp */

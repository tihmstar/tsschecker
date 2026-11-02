//
//  FirmwareAPI_IPSWME.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef FirmwareAPI_IPSWME_hpp
#define FirmwareAPI_IPSWME_hpp

#include <tsschecker/FirmwareAPI.hpp>
#include <stdint.h>
#include <vector>
#include <map>
#include <memory>
#include <string>

struct jssytok;
namespace tihmstar{
namespace tsschecker {
//firmware lists from the ipsw.me v4 API (https://api.ipsw.me/v4/)
class FirmwareAPI_IPSWME : public FirmwareAPI {
    struct JsonDoc;
    bool _ota;
    bool _beta;
    bool _readCache;

    std::map<std::string,std::unique_ptr<JsonDoc>> _responses; //API endpoint -> parsed response
    std::vector<std::string> _devicesCache;
    std::map<std::string,std::vector<firmwareVersion>> _versionsCache;

    const jssytok *fetch(const std::string &endpoint);
public:
    FirmwareAPI_IPSWME(bool ota = false, bool beta = false);
    virtual ~FirmwareAPI_IPSWME() override;
    
    virtual void load() override;
    virtual void loadcache() override;
    virtual void storecache() override;
    
    virtual std::vector<std::string> listDevices() override;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) override;
};
};
};

#endif /* FirmwareAPI_IPSWME_hpp */

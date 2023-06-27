//
//  FirmwareAPI.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef FirmwareAPI_hpp
#define FirmwareAPI_hpp

#include <stdint.h>
#include <vector>
#include <iostream>
#include <tsschecker/tsschecker.hpp>

namespace tihmstar{
namespace tsschecker {
class FirmwareAPI{
public:
    using firmwareVersion = tihmstar::tsschecker::firmwareVersion;
protected:
    std::string getCachePath();
public:
    virtual ~FirmwareAPI() = default;
    
    virtual void load() = 0;
    virtual void loadcache() = 0;
    virtual void storecache() = 0;

    virtual std::vector<std::string> listDevices() = 0;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) = 0;
    virtual firmwareVersion getURLForDeviceAndBuild(uint32_t cpid, uint32_t bdid, std::string version = "", std::string build = "") = 0;
    
    firmwareVersion getURLForDeviceAndBuild(std::string device, std::string version = "", std::string build = "");
    firmwareVersion getURLForBoardAndBuild(std::string board, std::string version = "", std::string build = "");
};
};
};

#endif /* FirmwareAPI_hpp */

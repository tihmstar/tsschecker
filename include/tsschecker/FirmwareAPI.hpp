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
    //fills in f.url when the list doesn't carry it. Returns false when the firmware has no file for device
    virtual bool resolveFirmwareURL(firmwareVersion &f, const std::string &device);
public:
    virtual ~FirmwareAPI() = default;
    
    virtual void load() = 0;
    virtual void loadcache() = 0;
    virtual void storecache() = 0;

    virtual std::vector<std::string> listDevices() = 0;
    virtual std::vector<firmwareVersion> listVersionsForDevice(std::string device) = 0;

    //empty version and build select the newest firmware
    firmwareVersion getURLForDeviceAndBuild(std::string device, std::string version = "", std::string build = "");
    firmwareVersion getURLForDeviceAndBuild(uint32_t cpid, uint32_t bdid, std::string version = "", std::string build = "");
    firmwareVersion getURLForBoardAndBuild(std::string board, std::string version = "", std::string build = "");
};
};
};

#endif /* FirmwareAPI_hpp */

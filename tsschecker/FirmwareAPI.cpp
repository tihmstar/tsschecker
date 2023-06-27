//
//  FirmwareAPI.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI.hpp>
#include <tsschecker/tsschecker.hpp>

#include <libgeneral/macros.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

using namespace tihmstar::tsschecker;

#pragma mark protected
std::string FirmwareAPI::getCachePath(){
#ifdef _WIN32
    std::string ret = getenv("APPDATA");
    ret += "/Local/Temp/tsschecker/";
    retassure(!mkdir(ret.c_str()) || errno == EEXIST, "Failed to mkdir '%s'",ret.c_str());
#else
    std::string ret = "/tmp/tsschecker/";
    retassure(!mkdir(ret.c_str(), 0755) || errno == EEXIST, "Failed to mkdir '%s'",ret.c_str());
#endif
    return ret;
}


#pragma mark public
FirmwareAPI::firmwareVersion FirmwareAPI::getURLForDeviceAndBuild(std::string device, std::string version, std::string build){
    reterror("todo");
}

FirmwareAPI::firmwareVersion FirmwareAPI::getURLForBoardAndBuild(std::string board, std::string version, std::string build){
    uint32_t cpid = 0;
    uint32_t bdid = 0;
    cpid = tsschecker::getCPIDForBoardType(board.c_str());
    bdid = tsschecker::getBDIDForBoardType(board.c_str());
    return getURLForDeviceAndBuild(cpid, bdid, version, build);
}

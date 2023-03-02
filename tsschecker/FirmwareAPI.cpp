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
    std::string ret = "/tmp/tsschecker/";
    retassure(!mkdir(ret.c_str(), 0755) || errno == EEXIST, "Failed to mkdir '%s'",ret.c_str());
    return ret;
}


#pragma mark public
FirmwareAPI::firmwareVersion FirmwareAPI::getURLForDeviceAndBuild(std::string device, std::string version, std::string build){
    reterror("todo");
}

FirmwareAPI::firmwareVersion FirmwareAPI::getURLForBoardAndBuild(std::string board, std::string version, std::string build){
    uint32_t cpid = 0;
    uint32_t bdid = 0;
    retassure(cpid = tsschecker::getCPIDForBoardType(board.c_str()), "Failed to get cpid for board '%s'",board.c_str());
    retassure(bdid = tsschecker::getBDIDForBoardType(board.c_str()), "Failed to get bdid for board '%s'",board.c_str());
    return getURLForDeviceAndBuild(cpid, bdid, version, build);
}

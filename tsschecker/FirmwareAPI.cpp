//
//  FirmwareAPI.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI.hpp>
#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TSSException.hpp>

#include <libgeneral/macros.h>

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

using namespace tihmstar::tsschecker;

#pragma mark protected
static void makeDirs(const std::string &path){
    //errors on intermediate components (existing dirs, drive letters) don't matter, only the final stat does
    for (size_t pos = path.find_first_of("/\\", 1); pos != std::string::npos; pos = path.find_first_of("/\\", pos+1)) {
        std::string dir = path.substr(0, pos);
#ifdef _WIN32
        mkdir(dir.c_str());
#else
        mkdir(dir.c_str(), 0700);
#endif
    }
    struct stat st = {};
    retassure(!stat(path.c_str(), &st) && S_ISDIR(st.st_mode), "Failed to create directory '%s'",path.c_str());
}

std::string FirmwareAPI::getCachePath(){
    std::string ret;
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    retassure(base, "LOCALAPPDATA not set");
    ret = base;
#else
    if (const char *xdg = getenv("XDG_CACHE_HOME")) {
        ret = xdg;
    }else{
        const char *home = getenv("HOME");
        retassure(home, "HOME not set");
        ret = home;
        ret += "/.cache";
    }
#endif
    ret += "/tsschecker/";
    makeDirs(ret);
    return ret;
}


#pragma mark public
FirmwareAPI::firmwareVersion FirmwareAPI::getURLForDeviceAndBuild(std::string device, std::string version, std::string build){
    const char *board = tsschecker::getBoardTypeFromProductType(device.c_str());
    retcustomassure(TSSException_DBLookupFailed, board, "Failed to get boardtype for '%s'",device.c_str());
    return getURLForBoardAndBuild(board, version, build);
}

FirmwareAPI::firmwareVersion FirmwareAPI::getURLForBoardAndBuild(std::string board, std::string version, std::string build){
    uint32_t cpid = 0;
    uint32_t bdid = 0;
    cpid = tsschecker::getCPIDForBoardType(board.c_str());
    bdid = tsschecker::getBDIDForBoardType(board.c_str());
    return getURLForDeviceAndBuild(cpid, bdid, version, build);
}

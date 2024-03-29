//
//  tsschecker.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef tsschecker_hpp
#define tsschecker_hpp

#include <tsschecker/TssRequest.hpp>
#include <libgeneral/Mem.hpp>
#include <plist/plist.h>
#include <vector>
#include <functional>
#include <stdint.h>

namespace tihmstar {
namespace tsschecker {
    enum nonceType{
        kNonceTypeNone = 0,
        kNonceTypeSHA1,
        kNonceTypeSHA384
    };

    struct firmwareVersion{
        std::string version;
        std::string build;
        std::string url;
    };

#pragma mark parsers
    uint64_t parseECID(const char *ecid);
    tihmstar::Mem parseHex(const char *hexstr);
    firmwareVersion firmwareVersionFromBuildManifest(plist_t pBuildManifest);

#pragma mark downloaders
    tihmstar::Mem downloadFile(const char *url);
    tihmstar::Mem downloadFileFromIPSW(const char *ipswurl, const char *path);
    plist_t getBuildManifestFromUrl(const char *ipswurl);
    firmwareVersion getLatestFirmwareForDevice(uint32_t cpid, uint32_t bdid, bool ota = false);

#pragma mark file handling
    plist_t readPlist(const char *path);
    void writePlist(const char *path, plist_t plist);

    std::string getTicketSavePathFromRequest(const char *path, const TssRequest &req);

#pragma mark printing
    void printListOfDevices(std::vector<std::string> devicesList);
    void printListOfVersions(std::vector<std::string> versionList);

#pragma mark device database
    const char *getBoardTypeFromProductType(const char *productType);
    const char *getBoardTypeFromCPIDandBDID(uint32_t cpid, uint32_t bdid);
    const char *getProductTypeFromBoardType(const char *boardType);
    const char *getProductTypeFromCPIDandBDID(uint32_t cpid, uint32_t bdid);
    uint32_t getCPIDForBoardType(const char *boardType);
    uint32_t getBDIDForBoardType(const char *boardType);
    uint32_t getCPIDForProductType(const char *productType);
    uint32_t getBDIDForProductType(const char *productType);
    nonceType nonceTypeForCPID(uint32_t cpid);
    int64_t getGoldCertIDForDevice(const char *productType);
    size_t getSNUMLenForDevice(const char *productType);

#pragma mark plist manipulation functions
    void *iterateOverPlistElementsInArray(plist_t array, std::function<void *(plist_t)> cb);
    void *iterateOverPlistElementsInDict(plist_t dict, std::function<void *(const char *,plist_t)> cb);
    void dumpplist(plist_t p);
    
    plist_t buildIdentityFromRestorePlist(plist_t restorePlist);
};
};

#endif /* tsschecker_hpp */

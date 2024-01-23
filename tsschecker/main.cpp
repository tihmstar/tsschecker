//
//  main.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI_IPSWME.hpp>
#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TssRequest.hpp>
#include <tsschecker/TSSException.hpp>

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>

#include <getopt.h>
#include <time.h>
#include <string.h>



using namespace tihmstar::tsschecker;

enum CacheSelector {
    kNoCache = 0,
    kCacheLoadAndStore = 1,
    kCacheStoreOnly = 2
};

enum RequestSelector {
    kRequestSelectorDefault = 0,
    kRequestSelectorDefaultBasebandNo = 1,
    kRequestSelectorDefaultBasebandOnly = 2,
    
    kRequestSelectorLastArg = 2
};

static struct option longopts[] = {
    /* Short opts */
    { "help",               no_argument,       NULL, 'h' },
    { "no-baseband",        optional_argument, NULL, 'b' },
    { "boardconfig",        required_argument, NULL, 'B' },
    { "cache",              optional_argument, NULL, 'c' },
    { "device",             required_argument, NULL, 'd' },
    { "ecid",               required_argument, NULL, 'e' },
    { "generator",          required_argument, NULL, 'g' },
    { "ios",                required_argument, NULL, 'i' },
    { "latest",             no_argument,       NULL, 'l' },
    { "build-manifest",     required_argument, NULL, 'm' },
    { "apnonce",            required_argument, NULL, 'N' },
    { "ota",                no_argument,       NULL, 'o' },
    { "save",               no_argument,       NULL, 's' },
    { "sepnonce",           required_argument, NULL, 'S' },
    { "update-install",     optional_argument, NULL, 'u' },
    { "buildid",            required_argument, NULL, 'Z' },
    { "buildid",            required_argument, NULL, 'Z' },
    { "variant",            required_argument, NULL, 'V' },
    { "verbose",            no_argument,       NULL, 'v' },

    /* Long opts */
    { "bbsnum",             required_argument, NULL,  0  },
    { "beta",               no_argument,       NULL,  0  },
    { "components",         required_argument, NULL,  0  },

    /* Info & Listing */
    { "list-devices",       no_argument,       NULL,  0  },
    { "list-versions",      no_argument,       NULL,  0  },
    { "list-builds",        no_argument,       NULL,  0  },

    /* Debugging */
    { "print-tss-request",  no_argument,       NULL,  0  },
    { "print-tss-response", no_argument,       NULL,  0  },
    { "raw",                required_argument, NULL,  0  },

    { NULL, 0, NULL, 0 }
};


void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    /* Short opts */
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("  -b, --no-baseband\t\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("  -B, --boardconfig BOARD \tspecific boardconfig instead of device model (eg. n61ap)\n");
    printf("  -d, --device MODEL\t\tspecific device by its model (eg. iPhone4,1)\n");
    printf("  -e, --ecid ECID\t\tmanually specify ECID to be used for fetching signing tickets, instead of using random ones\n");
    printf("                 \t\tECID must be either DEC or HEX eg. 5482657301265 or ab46efcbf71\n");
    printf("  -g, --generator GEN\t\tmanually specify generator in format 0x%%16llx\n");
    printf("  -i, --ios VERSION\t\tspecific firmware version (eg. 6.1.3)\n");
    printf("  -l, --latest\t\t\tuse latest public firmware version instead of manually specifying one\n");
    printf("  -m, --build-manifest\t\tmanually specify buildmanifest (can be used with -d)\n");
    printf("  -N, --apnonce NONCE\t\tmanually specify ApNonce instead of using random one (not required for saving blobs)\n");
    printf("  -o, --ota\t\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -s, --save [path]\t\tsave fetched shsh blobs (mostly makes sense with -e)\n");
    printf("  -S, --sepnonce <NONCE>\tmanually specify SepNonce instead of using random one (not required for saving blobs)\n");
    printf("  -u, --update-install\t\trequest update ticket instead of erase\n");
    printf("                 \t\tespecially useful with -s and -e for saving signing tickets\n");
    printf("  -V  --variant <VARIANT>\tspecify restore variant\n");
    printf("  -Z  --buildid <BUILD \t\tspecific buildid instead of firmware version (eg. 13C75)\n");

    /* Long opts */
    printf("      --bbsnum SNUM\t\tmanually specify BbSNUM in HEX for saving valid BBTicket (not required for saving blobs)\n");
    printf("      --beta\t\t\trequest ticket for beta instead of normal release (use with -o)\n");
    printf("      --cache\t\t\tCache requests to firmware api\n");
    printf("      --components <C1,C2,C3..> Request APTicket only for the following components\n");

    /* Info & Listing */
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-versions\t\tlist all known firmware versions\n");

    /* Debugging */
    printf("      --print-tss-request\tprint TSS request that will be sent to Apple\n");
    printf("      --print-tss-response\tprint TSS response that come from Apple\n");
    printf("      --raw\t\t\tsend raw file to Apple's TSS server (useful for debugging)\n\n");
}


MAINFUNCTION
int main_r(int argc, const char * argv[]) {
    info("%s",VERSION_STRING);
    int retval = 0;

    int opt = 0;
    int optindex = 0;
    
    bool isOta = false;
    CacheSelector useCache = kNoCache;
    
    bool doListDevices = false;
    bool doListVersions = false;
    bool doListBuilds = false;

    bool doPrintTssRequest = false;
    bool doPrintTssResponse = false;

    bool useLatestVersion = false;
    RequestSelector requestSelection = kRequestSelectorDefault;

    const char *productType = NULL;
    const char *boardType = NULL;
    const char *apnonce = NULL;
    const char *sepnonce = NULL;
    const char *rawRequestPath = NULL;
    const char *savePath = NULL;
    const char *buildManifestPath = NULL;

    std::string versionNumber;
    std::string buildNumber;
    std::string restoreVariant;

    std::vector<std::string> whitelistComponents;
    
    uint64_t ecid = 0;
    uint64_t generator = 0;

    while ((opt = getopt_long(argc, (char * const*)argv, "hb::B:c::d:e:d:i:lm:N:os::S:u::Z:vV:", longopts, &optindex)) >= 0) {
        switch (opt) {
            case 0:
            {
                std::string curopt = longopts[optindex].name;
                if (curopt == "components") {
                    std::string components = optarg;
                    ssize_t cpos = 0;
                    while ((cpos = components.find(",")) != std::string::npos) {
                        std::string scomp = components.substr(0,cpos);
                        components = components.substr(cpos+1);
                        whitelistComponents.push_back(scomp);
                    }
                    whitelistComponents.push_back(components);
                }else if (curopt == "list-devices") {
                    doListDevices = true;
                } else if (curopt == "list-versions") {
                    doListVersions = true;
                } else if (curopt == "list-builds") {
                    doListBuilds = true;
                } else if (curopt == "print-tss-request") {
                    doPrintTssRequest = true;
                } else if (curopt == "print-tss-response") {
                    doPrintTssResponse = true;
                } else if (curopt == "raw") {
                    rawRequestPath = optarg;
                }
                break;
            }
            case 'b': //long option "no-baseband"
                if (optarg) {
                    requestSelection = (RequestSelector)atoi(optarg);
                    retassure(requestSelection >= kRequestSelectorDefault && requestSelection <= kRequestSelectorLastArg, "requestSelection option out of range!");
                }else{
                    requestSelection = kRequestSelectorDefaultBasebandNo;
                }
                break;
                
            case 'B': //long option "boardconfig"
                boardType = optarg;
                break;
                
            case 'c': //long option "cache"
            {
                int l_cache = optarg ? atoi(optarg)-1 : (int)useCache;
                if (++l_cache > kCacheStoreOnly) l_cache = kCacheStoreOnly;
                useCache = (CacheSelector)l_cache;
            }
                break;
                
            case 'd': //long option "device"
                productType = optarg;
                break;

            case 'e': //long option "ecid"
                ecid = parseECID(optarg);
                break;

            case 'g': //long option "generator"
                generator = parseECID(optarg);
                break;

            case 'i': //long option "ios"
                versionNumber = optarg;
                break;

            case 'l': //long option "latest"
                useLatestVersion = true;
                break;

            case 'm': //long option "build-manifest"
                buildManifestPath = optarg;
                break;

            case 'N': //long option "apnonce"
                apnonce = optarg;
                break;

            case 'o': //long option "ota"
                isOta = true;
                break;

            case 'S': //long option "sepnonce"
                sepnonce = optarg;
                break;

            case 's': //long option "save"
                if (!(savePath = optarg)){
                    savePath = "./";
                }
                break;

            case 'u': //long option "upgrade-install"
                restoreVariant = "Customer Upgrade Install";
                break;
                
            case 'V': //long option "sepnonce"
                restoreVariant = optarg;
                break;
                
            case 'Z': //long option "buildid"
                buildNumber = optarg;
                break;

            case 'h': //long option "help"
                cmd_help();
                return 0;
                
            default:
                cmd_help();
                return -1;
        }
    }
    
    if (argc < 2) {
        //no arguments specified
        cmd_help();
        return 0;
    }
    
    if (rawRequestPath) {
        char *rsp = NULL;
        cleanup([&]{
            safeFree(rsp);
        });
        auto req = tihmstar::readFile(rawRequestPath);
        if (doPrintTssRequest) printf("\nrequest:\n%.*s\n",(int)req.size(),(char*)req.data());
        rsp = TssRequest::TssSendRawBuffer((char*)req.data(), req.size());
        if (doPrintTssResponse) printf("response:\n%s\n",rsp);
        if (savePath && rsp) {
            tihmstar::writeFile(savePath, rsp, strlen(rsp));
            info("Response saved to '%s'",savePath);
        }
        return 0;
    }

    FirmwareAPI *fapi = nullptr;
    cleanup([&]{
        if (fapi) {
            try{
                if (useCache >= kCacheLoadAndStore){
                    fapi->storecache();
                }
            }catch(tihmstar::exception &e){
                error("Failed to store cache with error:\n%s",e.dumpStr().c_str());
            }catch(...){
                error("[FATAL] Failed to store cache with UNKNOWN ERROR");
            }
        }
        safeDelete(fapi);
    });
    if (!buildManifestPath) {
        fapi = new FirmwareAPI_IPSWME(isOta);
        {
            //load firmwares
            bool isLoaded = false;
            try {
                if (useCache == kCacheLoadAndStore){
                    fapi->loadcache();
                    isLoaded = true;
                }
            } catch (tihmstar::exception &e) {
                warning("Failed to load cache with error:\n%s",e.dumpStr().c_str());
            }
            if (!isLoaded) fapi->load();
        }
    }
    
    if (fapi && doListDevices) {
        auto devs = fapi->listDevices();
        printListOfDevices(devs);
        return 0;
    }
    
    if (boardType && !productType){
        productType = getProductTypeFromBoardType(boardType);
    }else if (productType && !boardType){
        boardType = getBoardTypeFromProductType(productType);
        if (!boardType){
            error("Can't get board type for '%s'. Please manually specify boardtype",productType);
            return -2;
        }
    } else if (!productType && !boardType){
        error("No device specified!");
        return -1;
    }
    info("Identified device as %s %s cpid:0x%x bdid:0x%x",productType,boardType,getCPIDForBoardType(boardType),getBDIDForBoardType(boardType));
    if (!ecid) {
        warning("No ecid specified, generating random ecid!");
        srand((unsigned)time(NULL));
        ecid = ((uint64_t)rand() << 32) | rand();
    }
    if (fapi && (doListVersions || doListBuilds)){
        if (!productType){
            error("Please specify a device for this option. Use -h for more help");
            return -1;
        }
        auto vers = fapi->listVersionsForDevice(productType);
        std::vector<std::string> vNum;
        if (doListVersions) {
            for (auto v : vers) vNum.push_back(v.version);
        }else if (doListBuilds){
            for (auto v : vers) vNum.push_back(v.build);
        }else{
            reterror("unexpected case!");
        }
        printf("The following versions are known for %s:\n",productType);
        printListOfVersions(vNum);
    }else{
        plist_t p_BuildManifest = NULL;
        plist_t rsp = NULL;
        cleanup([&]{
            safeFreeCustom(rsp, plist_free);
            safeFreeCustom(p_BuildManifest, plist_free);
        });

        FirmwareAPI::firmwareVersion fvers;
        
        if (buildManifestPath){
            retassure(p_BuildManifest = readPlist(buildManifestPath), "Failed to load plist from '%s'",buildManifestPath);
            fvers = firmwareVersionFromBuildManifest(p_BuildManifest);
            info("Got Firmware %s %s",fvers.version.c_str(),fvers.build.c_str());
        }else if (fapi) {
            if (versionNumber.size() == 0 && buildNumber.size() == 0 && !useLatestVersion) {
                error("Please specify a firmware version or BuildID for this option");
                return -1;
            }
            if (boardType){
                fvers = fapi->getURLForBoardAndBuild(boardType, versionNumber, buildNumber);
            } else if (productType) {
                fvers = fapi->getURLForDeviceAndBuild(productType, versionNumber, buildNumber);
            }
            info("Got Firmware %s %s %s\n",fvers.version.c_str(),fvers.build.c_str(),fvers.url.c_str());
            p_BuildManifest = getBuildManifestFromUrl(fvers.url.c_str());
        }
        
        retassure(p_BuildManifest, "Failed to load BuildManifest.plist");
        
        TssRequest req(p_BuildManifest,restoreVariant);
        
        req.setDeviceVals(getCPIDForBoardType(boardType), getBDIDForBoardType(boardType));
        req.setEcid(ecid);
        if (requestSelection != kRequestSelectorDefaultBasebandOnly) {
            req.addDefaultAPTagsToRequest();
            if (whitelistComponents.size() == 0) {
                req.addAllAPComponentsToRequest();
//                req.addYonkersComponentsToRequest();
            }else{
                for (auto comp : whitelistComponents) {
                    req.addComponent(comp.c_str());
                }
            }

            if (!apnonce && !generator) {
                req.setNonceGenerator(*(uint64_t*)"tihmstar");
            }else{
                if (generator) req.setNonceGenerator(generator);
                if (apnonce){
                    if (*apnonce) {
                        req.setAPNonce(parseHex(apnonce));
                    }else{
                        req.unsetAPNonce();
                    }
                }
            }
            if (sepnonce) req.setSEPNonce(parseHex(sepnonce));
        }else{
            info("Requesting baseband-only ticket");
        }
        
        if (requestSelection == kRequestSelectorDefaultBasebandNo){
            info("Not requesting baseband ticket");
        } else{
            try {
                req.addBasebandComponentsToRequest();
                try {
                    req.setDefaultBbGoldCertId();
                    req.setRandomSNUM();
                } catch (tihmstar::exception &e) {
                    error("Failed to construct baseband requste with error:\n%s",e.dumpStr().c_str());
                    if (requestSelection == kRequestSelectorDefaultBasebandOnly) throw;
                    warning("Skipping requesting baseband ticket due to previous errors");
                }
            } catch (tihmstar::TSSException_missingValue &e) {
                if ((strncmp(e.keyname().c_str(), "Bb", 2) != 0 && strcmp(e.keyname().c_str(), "BasebandFirmware") != 0) && requestSelection != kRequestSelectorDefaultBasebandOnly) throw;
                warning("Skipping requesting baseband ticket, because required basenand values could not be extracted from manifest (Missing key: '%s')",e.keyname().c_str());
            }
        }
        
        if (doPrintTssRequest) req.dumpRequest();
        
        try {
            rsp = req.getTSSResponce();
        } catch (tihmstar::TSSException_NoTicket &e) {
            //
        }
        if (rsp) {
            if (doPrintTssResponse) dumpplist(rsp);
            info("%s %s for %s %s IS signed!",fvers.version.c_str(),fvers.build.c_str(),productType,boardType);
            retval = 0;
            
            if (savePath) {
                auto ticketpath = getTicketSavePathFromRequest(savePath, req);
                writePlist(ticketpath.c_str(), rsp);
                info("APTicket saved to '%s'",ticketpath.c_str());
            }
            
        }else{
            info("%s %s for %s %s is NOT signed!",fvers.version.c_str(),fvers.build.c_str(),productType,boardType);
            retval = 1;
        }
    }
    
    info("done");
    return retval;
}

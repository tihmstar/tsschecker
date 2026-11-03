//
//  main.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI_IPSWME.hpp>
#include <tsschecker/FirmwareAPI_AppleDB.hpp>
#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TssRequest.hpp>
#include <tsschecker/TSSException.hpp>

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>

#include <getopt.h>
#include <string.h>
#include <random>
#include <algorithm>



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
    { "erase-install",      no_argument,       NULL, 'E' },
    { "generator",          required_argument, NULL, 'g' },
    { "ios",                required_argument, NULL, 'i' },
    { "latest",             no_argument,       NULL, 'l' },
    { "build-manifest",     required_argument, NULL, 'm' },
    { "apnonce",            required_argument, NULL, 'N' },
    { "ota",                no_argument,       NULL, 'o' },
    { "save",               optional_argument, NULL, 's' },
    { "sepnonce",           required_argument, NULL, 'S' },
    { "update-install",     optional_argument, NULL, 'u' },
    { "buildid",            required_argument, NULL, 'Z' },
    { "variant",            required_argument, NULL, 'V' },
    { "verbose",            no_argument,       NULL, 'v' },

    /* Long opts */
    { "appledb",            no_argument,       NULL,  0  },
    { "bbsnum",             required_argument, NULL,  0  },
    { "bdid",               required_argument, NULL,  0  },
    { "bplist",             no_argument,       NULL,  0  },
    { "beta",               no_argument,       NULL,  0  },
    { "components",         required_argument, NULL,  0  },
    { "cpid",               required_argument, NULL,  0  },
    { "cryptex-nonce",      required_argument, NULL,  0  },
    { "cryptex-seed",       required_argument, NULL,  0  },

    /* Info & Listing */
    { "list-devices",       no_argument,       NULL,  0  },
    { "list-versions",      no_argument,       NULL,  0  },
    { "list-builds",        no_argument,       NULL,  0  },

    /* Debugging */
    { "print-tss-request",  no_argument,       NULL,  0  },
    { "print-tss-response", no_argument,       NULL,  0  },
    { "raw",                required_argument, NULL,  0  },
    { "server-url",         required_argument, NULL,  0  },

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
    printf("  -E, --erase-install\t\trequest only the erase ticket (no extra update and nonce-less tickets when saving)\n");
    printf("  -g, --generator GEN\t\tmanually specify generator in format 0x%%16llx\n");
    printf("  -i, --ios VERSION\t\tspecific firmware version (eg. 6.1.3)\n");
    printf("  -l, --latest\t\t\tuse latest public firmware version instead of manually specifying one\n");
    printf("  -m, --build-manifest\t\tmanually specify buildmanifest (can be used with -d)\n");
    printf("  -N, --apnonce NONCE\t\tmanually specify ApNonce instead of using random one (not required for saving blobs)\n");
    printf("  -o, --ota\t\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -s, --save [path]\t\tsave fetched shsh blobs to path (default: current directory, mostly makes sense with -e)\n");
    printf("  -S, --sepnonce <NONCE>\tmanually specify SepNonce instead of using random one (not required for saving blobs)\n");
    printf("  -u, --update-install\t\trequest update ticket instead of erase\n");
    printf("                 \t\tespecially useful with -s and -e for saving signing tickets\n");
    printf("  -v, --verbose\t\t\tprint TSS request and response\n");
    printf("  -V  --variant <VARIANT>\tspecify restore variant\n");
    printf("  -Z  --buildid <BUILD \t\tspecific buildid instead of firmware version (eg. 13C75)\n");

    /* Long opts */
    printf("      --appledb\t\tget firmware lists from AppleDB instead of ipsw.me\n");
    printf("                 \t\t(used automatically when ipsw.me has no firmwares for the device)\n");
    printf("      --bbsnum SNUM\t\tmanually specify BbSNUM in HEX for saving valid BBTicket (not required for saving blobs)\n");
    printf("      --bdid <bdid> Manually specify bdid\n");
    printf("      --bplist\t\t\tsave shsh blobs as binary plist\n");
    printf("      --beta\t\t\tinclude beta firmwares in firmware lists (OTA only, use with -o)\n");
    printf("  -c, --cache\t\t\tCache requests to firmware api (-c -c or --cache=2 only stores the cache)\n");
    printf("      --components <C1,C2,C3..> Request APTicket only for the following components\n");
    printf("      --cpid <cpid> Manually specify cpid\n");
    printf("      --cryptex-nonce NONCE\talso request a Cryptex1 ticket for this Cryptex1 nonce (32 bytes HEX, needs --cryptex-seed)\n");
    printf("      --cryptex-seed SEED\tCryptex1 seed (16 bytes HEX) stored with the Cryptex1 ticket in the saved blob\n");

    /* Info & Listing */
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-versions\t\tlist all known firmware versions\n");
    printf("      --list-builds\t\tlist all known firmware builds\n");

    /* Debugging */
    printf("      --print-tss-request\tprint TSS request that will be sent to Apple\n");
    printf("      --print-tss-response\tprint TSS response that come from Apple\n");
    printf("      --raw\t\t\tsend raw file to Apple's TSS server (useful for debugging)\n");
    printf("      --server-url URL\t\tsend TSS requests to URL instead of Apple's servers\n\n");
}


MAINFUNCTION
int main_r(int argc, const char * argv[]) {
    info("%s",VERSION_STRING);
    int retval = 0;

    int opt = 0;
    int optindex = 0;
    
    bool isOta = false;
    bool includeBeta = false;
    CacheSelector useCache = kNoCache;
    bool useAppleDB = false;
    
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
    const char *bbsnum = NULL;
    const char *cryptexNonceStr = NULL;
    const char *serverURL = NULL;
    bool saveBplist = false;
    const char *cryptexSeedStr = NULL;

    std::string versionNumber;
    std::string buildNumber;
    std::string restoreVariant;

    std::vector<std::string> whitelistComponents;
    
    uint64_t ecid = 0;
    uint64_t generator = 0;
    uint32_t cpid = 0;
    uint32_t bdid = 0;

    while ((opt = getopt_long(argc, (char * const*)argv, "hb::B:c::d:e:Eg:i:lm:N:os::S:u::Z:vV:", longopts, &optindex)) >= 0) {
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
                }else if (curopt == "appledb") {
                    useAppleDB = true;
                }else if (curopt == "bbsnum") {
                    bbsnum = optarg;
                }else if (curopt == "beta") {
                    includeBeta = true;
                }else if (curopt == "bplist") {
                    saveBplist = true;
                }else if (curopt == "server-url") {
                    serverURL = optarg;
                }else if (curopt == "bdid") {
                    bdid = (uint32_t)parseECID(optarg);
                }else if (curopt == "cpid") {
                    cpid = (uint32_t)parseECID(optarg);
                }else if (curopt == "cryptex-nonce") {
                    cryptexNonceStr = optarg;
                }else if (curopt == "cryptex-seed") {
                    cryptexSeedStr = optarg;
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
                //-c / --cache=1 loads and stores the cache, -cc / --cache=2 only stores it
                int l_cache = optarg ? atoi(optarg) : (int)useCache + 1;
                useCache = (CacheSelector)std::min(std::max(l_cache, (int)kNoCache), (int)kCacheStoreOnly);
            }
                break;
                
            case 'd': //long option "device"
                productType = optarg;
                break;

            case 'e': //long option "ecid"
                ecid = parseECID(optarg);
                break;

            case 'E': //long option "erase-install"
                restoreVariant = RESTORE_VARIANT_ERASE_INSTALL;
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
                    //getopt only takes optional arguments attached ("-s<path>"), also accept "-s <path>"
                    if (optind < argc && argv[optind][0] != '-') savePath = argv[optind++];
                    else savePath = "./";
                }
                break;

            case 'u': //long option "upgrade-install"
                restoreVariant = "Customer Upgrade Install";
                break;
                
            case 'V': //long option "variant"
                restoreVariant = optarg;
                break;

            case 'v': //long option "verbose"
                doPrintTssRequest = true;
                doPrintTssResponse = true;
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
        rsp = TssRequest::TssSendRawBuffer((char*)req.data(), req.size(), serverURL);
        if (doPrintTssResponse) printf("response:\n%s\n",rsp);
        if (savePath && rsp) {
            tihmstar::writeFile(savePath, rsp, strlen(rsp));
            info("Response saved to '%s'",savePath);
        }
        return 0;
    }

    tihmstar::Mem cryptexNonce;
    std::string cryptexSeed;
    if (cryptexNonceStr || cryptexSeedStr) {
        //a Cryptex1 ticket can only be used later together with the seed that produces its nonce
        retassure(cryptexNonceStr && cryptexSeedStr, "--cryptex-nonce and --cryptex-seed must be used together");
        cryptexNonce = parseHex(cryptexNonceStr);
        retassure(cryptexNonce.size() == 32, "Cryptex1 nonce must be 32 bytes, got %d",cryptexNonce.size());
        auto seed = parseHex(cryptexSeedStr);
        retassure(seed.size() == 16, "Cryptex1 seed must be 16 bytes, got %d",seed.size());
        cryptexSeed = "0x";
        for (size_t i=0; i<seed.size(); i++) {
            char buf[4] = {};
            snprintf(buf, sizeof(buf), "%02x",seed.data()[i]);
            cryptexSeed += buf;
        }
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
    auto loadFirmwareAPI = [&]{
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
    };
    if (!buildManifestPath) {
        if (useAppleDB) fapi = new FirmwareAPI_AppleDB(isOta, includeBeta);
        else fapi = new FirmwareAPI_IPSWME(isOta, includeBeta);
        loadFirmwareAPI();
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
    } else if (!productType && !boardType){
        error("No device specified!");
        return -1;
    }
    if (!cpid || !bdid) {
        if (!boardType){
            error("Can't get board type for '%s'. Please manually specify boardtype or --cpid and --bdid",productType);
            return -2;
        }
        if (!cpid) cpid = getCPIDForBoardType(boardType);
        if (!bdid) bdid = getBDIDForBoardType(boardType);
    }
    const char *productTypeName = productType ? productType : "(unknown product)";
    const char *boardTypeName = boardType ? boardType : "(unknown board)";
    info("Identified device as %s %s cpid:0x%x bdid:0x%x",productTypeName,boardTypeName,cpid,bdid);
    if (!ecid) {
        warning("No ecid specified, generating random ecid!");
        //real ECIDs fit in 52 bits
        ecid = std::mt19937_64{std::random_device{}()}() & 0x000FFFFFFFFFFFFFULL;
    }
    //ipsw.me lacks devices without IPSWs (Watch6+) and OTAs for some new devices
    if (fapi && !useAppleDB && productType && [&]{
        try {
            return fapi->listVersionsForDevice(productType).empty();
        } catch (tihmstar::exception &e) {
            //ipsw.me answers 404 for devices it doesn't know
            debug("ipsw.me lookup failed:\n%s",e.dumpStr().c_str());
            return true;
        }
    }()) {
        info("ipsw.me has no firmwares for %s, using AppleDB",productType);
        if (useCache >= kCacheLoadAndStore) {
            try { fapi->storecache(); } catch (tihmstar::exception &e) { warning("Failed to store cache with error:\n%s",e.dumpStr().c_str()); }
        }
        safeDelete(fapi);
        useAppleDB = true;
        fapi = new FirmwareAPI_AppleDB(isOta, includeBeta);
        loadFirmwareAPI();
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
            if (productType){
                fvers = fapi->getURLForDeviceAndBuild(std::string(productType), versionNumber, buildNumber);
            } else {
                fvers = fapi->getURLForBoardAndBuild(boardType, versionNumber, buildNumber);
            }
            info("Got Firmware %s %s %s\n",fvers.version.c_str(),fvers.build.c_str(),fvers.url.c_str());
            p_BuildManifest = getBuildManifestFromUrl(fvers.url.c_str());
        }
        
        retassure(p_BuildManifest, "Failed to load BuildManifest.plist");
        
        auto buildRequest = [&](TssRequest &req, bool noNonce){
            //nonce-less tickets are requested without baseband, like the C tsschecker does
            RequestSelector selection = noNonce ? kRequestSelectorDefaultBasebandNo : requestSelection;
            req.setDeviceVals(cpid, bdid, false, productType);
            req.setEcid(ecid);
            if (selection != kRequestSelectorDefaultBasebandOnly) {
                req.addDefaultAPTagsToRequest();
                if (whitelistComponents.size() == 0) {
                    req.addAllAPComponentsToRequest();
    //                req.addYonkersComponentsToRequest();
                }else{
                    for (auto comp : whitelistComponents) {
                        req.addComponent(comp.c_str());
                    }
                }

                if (noNonce) {
                    req.setEmptyAPNonce();
                }else if (!apnonce && !generator) {
                    req.setNonceGenerator(0x726174736d686974); //"tihmstar" as little-endian uint64
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
        
            if (selection == kRequestSelectorDefaultBasebandNo){
                info("Not requesting baseband ticket");
            } else{
                try {
                    req.addBasebandComponentsToRequest();
                    try {
                        req.setDefaultBbGoldCertId();
                        if (bbsnum) req.setSNUM(parseHex(bbsnum));
                        else req.setRandomSNUM();
                    } catch (tihmstar::exception &e) {
                        error("Failed to construct baseband request with error:\n%s",e.dumpStr().c_str());
                        if (selection == kRequestSelectorDefaultBasebandOnly) throw;
                        warning("Skipping requesting baseband ticket due to previous errors");
                        //a partial baseband request makes TSS fail the whole request (status 161)
                        req.removeBasebandComponentsFromRequest();
                    }
                } catch (tihmstar::TSSException_missingValue &e) {
                    if ((strncmp(e.keyname().c_str(), "Bb", 2) != 0 && strcmp(e.keyname().c_str(), "BasebandFirmware") != 0) || selection == kRequestSelectorDefaultBasebandOnly) throw;
                    warning("Skipping requesting baseband ticket, because required baseband values could not be extracted from manifest (Missing key: '%s')",e.keyname().c_str());
                    req.removeBasebandComponentsFromRequest();
                }
            }
        };
        //returns NULL when TSS gives no ticket
        auto sendRequest = [&](TssRequest &req)->plist_t{
            if (doPrintTssRequest) req.dumpRequest();
            plist_t ret = NULL;
            try {
                ret = req.getTSSResponce(serverURL);
            } catch (tihmstar::TSSException_NoTicket &e) {
                //
            }
            if (ret && doPrintTssResponse) dumpplist(ret);
            return ret;
        };

        TssRequest req(p_BuildManifest,restoreVariant);
        buildRequest(req, false);
        rsp = sendRequest(req);
        if (rsp) {
            info("%s %s for %s %s IS signed!",fvers.version.c_str(),fvers.build.c_str(),productTypeName,boardTypeName);
            retval = 0;

            //futurerestore uses the updateInstall ticket for update restores (-u)
            if (savePath && restoreVariant.empty() && requestSelection != kRequestSelectorDefaultBasebandOnly
                && TssRequest::getVariantNameFromBuildIdentity(req.getSelectedBuildIdentity()) == RESTORE_VARIANT_ERASE_INSTALL) {
                info("Also requesting ticket for variant '%s'",RESTORE_VARIANT_UPGRADE_INSTALL);
                try {
                    TssRequest ureq(p_BuildManifest,RESTORE_VARIANT_UPGRADE_INSTALL);
                    buildRequest(ureq, false);
                    if (plist_t ursp = sendRequest(ureq)) {
                        plist_dict_remove_item(ursp, "generator");
                        plist_dict_set_item(rsp, "updateInstall", ursp);
                    }else{
                        warning("Failed to get ticket for variant '%s'",RESTORE_VARIANT_UPGRADE_INSTALL);
                    }
                } catch (tihmstar::exception &e) {
                    warning("Skipping update ticket:\n%s",e.dumpStr().c_str());
                }
            }

            //nonce-less ticket, saved as noNonce like the C tsschecker. TSS refuses it for many devices
            if (savePath && restoreVariant.empty() && requestSelection != kRequestSelectorDefaultBasebandOnly) {
                info("Also requesting ticket without a nonce");
                try {
                    TssRequest nreq(p_BuildManifest,restoreVariant);
                    buildRequest(nreq, true);
                    if (plist_t nrsp = sendRequest(nreq)) {
                        plist_dict_remove_item(nrsp, "generator");
                        plist_dict_set_item(rsp, "noNonce", nrsp);
                    }else{
                        info("TSS gives no nonce-less ticket for this device");
                    }
                } catch (tihmstar::exception &e) {
                    warning("Skipping nonce-less ticket:\n%s",e.dumpStr().c_str());
                }
            }

            if (cryptexNonce.size()) {
                info("Also requesting Cryptex1 ticket");
                try {
                    TssRequest creq(p_BuildManifest,restoreVariant);
                    creq.setDeviceVals(cpid, bdid, false, productType);
                    creq.setEcid(ecid);
                    creq.makeCryptex1Request(cryptexNonce);
                    plist_t crsp = sendRequest(creq);
                    if (crsp && plist_dict_get_item(crsp, "Cryptex1,Ticket")) {
                        info("Got Cryptex1 ticket");
                        plist_dict_set_item(rsp, "cryptexTicket", crsp);
                        plist_dict_set_item(rsp, "cryptexSeed", plist_new_string(cryptexSeed.c_str()));
                    }else{
                        safeFreeCustom(crsp, plist_free);
                        error("Failed to get Cryptex1 ticket");
                        retval = 2;
                    }
                } catch (tihmstar::exception &e) {
                    error("Failed to request Cryptex1 ticket:\n%s",e.dumpStr().c_str());
                    retval = 2;
                }
            }

            if (savePath) {
                auto ticketpath = getTicketSavePathFromRequest(savePath, req);
                writePlist(ticketpath.c_str(), rsp, saveBplist);
                info("APTicket saved to '%s'",ticketpath.c_str());
            }
            
        }else{
            info("%s %s for %s %s is NOT signed!",fvers.version.c_str(),fvers.build.c_str(),productTypeName,boardTypeName);
            retval = 1;
        }
    }
    
    info("done");
    return retval;
}

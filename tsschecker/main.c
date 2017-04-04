//
//  main.c
//  tsschecker
//
//  Created by tihmstar on 22.12.15.
//  Copyright © 2015 tihmstar. All rights reserved.
//

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "download.h"
#include "tsschecker.h"
#include "all_tsschecker.h"


#define FLAG_LIST_IOS       1 << 0
#define FLAG_LIST_DEVICES   1 << 1
#define FLAG_BUILDMANIFEST  1 << 2
#define FLAG_LATEST_IOS     1 << 3

int idevicerestore_debug;
#define reterror(code,a ...) {error(a); err = code; goto error;}


static struct option longopts[] = {
    { "list-devices",       no_argument,       NULL, '1' },
    { "list-ios",           no_argument,       NULL, '2' },
    { "build-manifest",     required_argument, NULL, 'm' },
    { "save-path",          required_argument, NULL, '3' },
    { "print-tss-request",  no_argument,       NULL, '4' },
    { "print-tss-response", no_argument,       NULL, '5' },
    { "beta",               no_argument,       NULL, '6' },
    { "nocache",            no_argument,       NULL, '7' },
    { "apnonce",            required_argument, NULL, '8' },
    { "sepnonce",           required_argument, NULL, '9' },
    { "device",             required_argument, NULL, 'd' },
    { "ios",                required_argument, NULL, 'i' },
    { "ecid",               required_argument, NULL, 'e' },
    { "help",               no_argument,       NULL, 'h' },
    { "no-baseband",        optional_argument, NULL, 'b' },
    { "ota",                no_argument,       NULL, 'o' },
    { "save",               no_argument,       NULL, 's' },
    { "latest",             no_argument,       NULL, 'l' },
    { "update-install",     optional_argument, NULL, 'u' },
    { "boardconfig",        required_argument, NULL, 'B' },
    { "buildid",            required_argument, NULL, 'Z' },
    { "debug",              no_argument,       NULL, '0' },
    { "upgrade",            no_argument,       NULL, '^' },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    
    printf("  -d, --device MODEL\t\tspecific device by its MODEL (eg. iPhone4,1)\n");
    printf("  -i, --ios VERSION\t\tspecific iOS version (eg. 6.1.3)\n");
    printf("      --buildid BUILDID\t\tspecific buildid instead of iOS version (eg. 13C75)\n");
    printf("      --boardconfig BOARD\tspecific boardconfig instead of iPhone model (eg. n61ap)\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("  -o, --ota\t\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -b, --no-baseband\t\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("  -m, --build-manifest\t\tmanually specify buildmanifest. (can be used with -d)\n");
    printf("  -s, --save\t\t\tsave fetched shsh blobs (mostly makes sense with -e)\n");
    printf("  -u, --update-install\t\t\trequest update ticket instead of erase\n");
    printf("  -l, --latest\t\t\tuse latest public iOS version instead of manually specifying one\n");
    printf("                 \t\tespecially useful with -s and -e for saving blobs\n");
    printf("  -e, --ecid ECID\t\tmanually specify ECID to be used for fetching blobs, instead of using random ones\n");
    printf("                 \t\tECID must be either dec or hex eg. 5482657301265 or ab46efcbf71\n");
    printf("      --apnonce NONCE\t\tmanually specify APNONCE instead of using random one (not required for saving blobs)\n");
    printf("      --sepnonce NONCE\t\tmanually specify SEPNONCE instead of using random one (not required for saving blobs)\n");
    printf("      --save-path PATH\t\tspecify path for saving blobs\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("      --beta\t\t\trequest ticket for beta instead of normal relase (use with -o)\n");
    printf("      --upgrade\t\t\tuse upgrade BuildIdentity, instead of erase\n");
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-ios\t\tlist all known ios versions\n");
    printf("      --nocache \t\tignore caches and redownload required files\n");
    printf("      --print-tss-request\n");
    printf("      --print-tss-response\n");
    printf("\n");
}

int64_t parseECID(const char *ecid){
    const char *ecidBK = ecid;
    int isHex = 0;
    int64_t ret = 0;
    
    //in case hex ecid only contains digits, specify with 0x1235
    if (strncmp(ecid, "0x", 2) == 0){
        isHex = 1;
        ecidBK = ecid+2;
    }
    
    while (*ecid && !isHex) {
        char c = *(ecid++);
        if (c >= '0' && c<='9') {
            ret *=10;
            ret += c - '0';
        }else{
            isHex = 1;
            ret = 0;
        }
    }
    
    if (isHex) {
        while (*ecidBK) {
            char c = *(ecidBK++);
            ret *=16;
            if (c >= '0' && c<='9') {
                ret += c - '0';
            }else if (c >= 'a' && c <= 'f'){
                ret += 10 + c - 'a';
            }else if (c >= 'A' && c <= 'F'){
                ret += 10 + c - 'A';
            }else{
                return 0; //ERROR parsing failed
            }
        }
    }
    
    return ret;
}

char *parseNonce(const char *nonce, size_t *parsedLen){
    char *ret = NULL;
    size_t retSize = 0;
    if (parseHex(nonce, parsedLen, NULL, &retSize))
        return NULL;
    ret = malloc(retSize);
    if (parseHex(nonce, parsedLen, ret, &retSize))
        return NULL;
    
    return ret;
}

int main(int argc, const char * argv[]) {
    int err = 0;
    int isSigned = 0;
    printf("Version: "VERSION_COMMIT_SHA" - "VERSION_COMMIT_COUNT"\n");
    
    dbglog = 1;
    idevicerestore_debug = 0;
    save_shshblobs = 0;
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    char *buildmanifest = 0;
    char *ecid = 0;
    
    char *apnonce = 0;
    char *sepnonce = 0;
    t_devicevals devVals = {0};
    t_iosVersion versVals = {0};
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    while ((opt = getopt_long(argc, (char* const *)argv, "d:i:e:m:B:hslbuo", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
            case 'd': // long option: "device"; can be called as short option
                devVals.deviceModel = strdup(optarg);
                break;
            case 'i': // long option: "ios"; can be called as short option
                if (versVals.version) reterror(-9, "[TSSC] parsing parameter failed!\n");
                versVals.version = strdup(optarg);
                break;
            case 'Z': // long option: "ios"; can be called as short option
                versVals.buildID = strdup(optarg);
                break;
            case 'B': // long option: "boardconfig"; can be called as short option
                devVals.deviceBoard = strdup(optarg);
                break;
            case 'e': // long option: "ecid"; can be called as short option
                ecid = optarg;
                break;
            case 'b': // long option: "no-baseband"; can be called as short option
                if (optarg) versVals.basebandMode = atoi(optarg);
                else versVals.basebandMode = kBasebandModeWithoutBaseband;
                break;
            case 'u': // long option: "update"; can be called as short option
                if (optarg) {
                    if ((devVals.installType = atoi(optarg)) > 2 || devVals.installType < 0){
                        warning("unknown installType %d. Setting installType to default (%d)\n",devVals.installType,devVals.installType = kInstallTypeDefault);
                    }
                }else
                    devVals.installType = kInstallTypeUpdate;
                if (devVals.installType)
                    printf("[TSSC] manually setting install type = %s\n",devVals.installType == kInstallTypeUpdate ? "Update" : "Erase");
                break;
            case 'l': // long option: "latest"; can be called as short option
                flags |= FLAG_LATEST_IOS;
                break;
            case 's': // long option: "save"; can be called as short option
                save_shshblobs = 1;
                break;
            case 'o': // long option: "ota"; can be called as short option
                versVals.isOta = 1;
                break;
            case '0': // only long option: "debug"
                idevicerestore_debug = 1;
                break;
            case '1': // only long option: "list-devices"
                flags |= FLAG_LIST_DEVICES;
                break;
            case '2': // only long option: "list-ios"
                flags |= FLAG_LIST_IOS;
                break;
            case 'm': // long option: "build-manifest"; can be called as short option
                flags |= FLAG_BUILDMANIFEST;
                buildmanifest = optarg;
                break;
            case '3': // only long option: "save-path"
                shshSavePath = optarg;
                break;
            case '4': // only long option: "print-tss-request"
                print_tss_request = 1;
                break;
            case '5': // only long option: "print-tss-response"
                print_tss_response = 1;
                break;
            case '6': // only long option: "beta"
                versVals.useBeta = 1;
                break;
            case '7': // only long option: "nocache"
                nocache = 1;
                break;
            case '8': // only long option: "apnonce"
                apnonce = optarg;
                break;
            case '9': // only long option: "sepnonce"
                sepnonce = optarg;
                break;
            case '^': // only long option: "upgrade"
                devVals.isUpgradeInstall = 1;
                break;

            default:
                cmd_help();
                return -1;
        }
    }
    char *firmwareJson = NULL;
    jsmntok_t *firmwareTokens = NULL;
    
    if (devVals.deviceBoard)
        for (int i=0; i<strlen(devVals.deviceBoard); i++)
            devVals.deviceBoard[i] = tolower(devVals.deviceBoard[i]);
    
    
    if (!devVals.deviceModel){
        if (devVals.deviceBoard){
            char *tmp = NULL;
            if ((tmp = (char*)getModelFromBoardconfig(devVals.deviceBoard)))
                devVals.deviceModel = strdup(tmp);
            else
                reterror(-25, "[TSSC] If you using --boardconfig please also specify devicemodel with -d\n");
        }
    }
    
    if (devVals.deviceModel){
        for (char *c = devVals.deviceModel; *c; c++)
            *c = tolower(*c); //make devicemodel lowercase
        //make devicemodel look nice. This is completely optional
        if (*devVals.deviceModel == 'i')
            devVals.deviceModel[1] = toupper(devVals.deviceModel[1]);
        else
            devVals.deviceModel[0] = toupper(devVals.deviceModel[0]);
    }
    
    if (ecid) {
        if ((devVals.ecid = parseECID(ecid)) == 0){
            reterror(-7, "[TSSC] manually specified ecid=%s, but parsing failed\n",ecid);
        }else{
            info("[TSSC] manually specified ecid to use, parsed \"%s\" to dec:%lld hex:%llx\n",ecid,devVals.ecid,devVals.ecid);
        }
    }
    if (apnonce) {
        if ((devVals.apnonce = parseNonce(apnonce,&devVals.parsedApnonceLen)) ){
            info("[TSSC] manually specified apnonce to use, parsed \"%s\" to hex:",apnonce);
            unsigned char *tmp = (unsigned char*)devVals.apnonce;
            for (int i=0; i< devVals.parsedApnonceLen; i++) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] manually specified apnonce=%s, but parsing failed\n",apnonce);
        }
    }
    if (sepnonce) {
        if ((devVals.sepnonce = parseNonce(sepnonce,&devVals.parsedSepnonceLen)) ){
            info("[TSSC] manually specified sepnonce to use, parsed \"%s\" to hex:",sepnonce);
            unsigned char *tmp = (unsigned char*)devVals.sepnonce;
            for (int i=0; i< devVals.parsedSepnonceLen; i++) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] manually specified sepnonce=%s, but parsing failed\n",sepnonce);
        }
    }
    
    if (!buildmanifest) { //no need to get firmares/ota json if specifying buildmanifest manually
    reparse:
        firmwareJson = (versVals.isOta) ? getOtaJson() : getFirmwareJson();
        if (!devVals.installType) //only set this if installType wasn't set manually
            devVals.isUpdateInstall = (versVals.isOta); //there are no erase installs over OTA
        if (!firmwareJson) reterror(-6,"[TSSC] could not get firmware.json\n");
        
        int cnt = parseTokens(firmwareJson, &firmwareTokens);
        if (cnt < 1){
            if (!nocache){
                warning("[TSSC] error parsing cached %s.json. Trying to redownload\n",(versVals.isOta) ? "ota" : "firmware");
                nocache = 1;
                goto reparse; //in case cached json is corrupt, try to redownload and reparse one more time.
            }
            reterror(-2,"[TSSC] parsing %s.json failed\n",(versVals.isOta) ? "ota" : "firmware");
        }
    }

    
    
    if (flags & FLAG_LATEST_IOS && !versVals.version){
        int versionCnt = 0;
        int i = 0;
            
        char **versions = getListOfiOSForDevice(firmwareJson, firmwareTokens, devVals.deviceModel, versVals.isOta, &versionCnt);
        if (!versionCnt) reterror(-8, "[TSSC] failed finding latest iOS. If you using --boardconfig please also specify devicemodel with -d ota=%d\n",versVals.isOta);
        char *bpos = NULL;
        while((bpos = strstr(versVals.version = strdup(versions[i++]),"[B]")) != 0){
            if (versVals.useBeta) break;
            free((char*)versVals.version);
            if (--versionCnt == 0) reterror(-9, "[TSSC] automatic iOS selection couldn't find non-beta iOS\n");
        }
        info("[TSSC] selecting latest iOS: %s\n",versVals.version);
        if (bpos) *bpos= '\0';
        if (versions) free(versions[versionCnt-1]),free(versions);
    }
    
    if (flags & FLAG_LIST_DEVICES) {
        printListOfDevices(firmwareJson, firmwareTokens);
    }else if (flags & FLAG_LIST_IOS){
        if (!devVals.deviceModel)
            reterror(-3,"[TSSC] please specify a device for this option\n\tuse -h for more help\n");

        printListOfiOSForDevice(firmwareJson, firmwareTokens, devVals.deviceModel, versVals.isOta);
    }else{
        //request ticket
        if (buildmanifest) {
            isSigned = isManifestSignedForDevice(buildmanifest, &devVals, &versVals);

        }else{
            if (!devVals.deviceModel) reterror(-3,"[TSSC] please specify a device for this option\n\tuse -h for more help\n");
            if (!versVals.version) reterror(-5,"[TSSC] please specify an iOS version or buildID for this option\n\tuse -h for more help\n");
            
            isSigned = isVersionSignedForDevice(firmwareJson, firmwareTokens, &versVals, &devVals);
        }
        
        if (isSigned >=0) printf("\n%s %s for device %s %s being signed!\n",(versVals.buildID) ? "Build" : "iOS" ,(versVals.buildID ? versVals.buildID : versVals.version),devVals.deviceModel, (isSigned) ? "IS" : "IS NOT");
        else putchar('\n'),error("[TSSC] checking tss status failed!\n");
    }
    
    
    
error:
    if (devVals.deviceBoard) free(devVals.deviceBoard);
    if (devVals.deviceModel) free(devVals.deviceModel);
    if (devVals.apnonce) free(devVals.apnonce);
    if (devVals.sepnonce) free(devVals.sepnonce);
    if (firmwareJson) free(firmwareJson);
    if (firmwareTokens) free(firmwareTokens);
    return err ? err : !isSigned;
#undef reterror
}

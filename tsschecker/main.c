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
#include "download.h"
#include "tsschecker.h"

#define FLAG_LIST_IOS       1 << 0
#define FLAG_LIST_DEVICES   1 << 1
#define FLAG_OTA            1 << 2
#define FLAG_NO_BASEBAND    1 << 3
#define FLAG_BUILDMANIFEST  1 << 4
#define FLAG_BETA           1 << 5

int dbglog;
int idevicerestore_debug;

static struct option longopts[] = {
    { "list-devices",         no_argument,       NULL, '1' },
    { "list-ios",             no_argument,       NULL, '2' },
    { "build-manifest",       required_argument, NULL, '3' },
    { "print-tss-request",    no_argument,       NULL, '4' },
    { "print-tss-response",   no_argument,       NULL, '5' },
    { "beta",                 no_argument,       NULL, '6' },
    { "nocache",              no_argument,       NULL, '7' },
    { "device",         required_argument, NULL, 'd' },
    { "ios",            required_argument, NULL, 'i' },
    { "ecid",           required_argument, NULL, 'e' },
    { "help",           no_argument,       NULL, 'h' },
    { "no-baseband",    no_argument,       NULL, 'b' },
    { "ota",    no_argument,       NULL, 'o' },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    
    printf("  -d, --device MODEL\tspecific device by its MODEL (eg. iPhone4,1)\n");
    printf("  -i, --ios VERSION\tspecific iOS version (eg. 6.1.3)\n");
    printf("  -h, --help\t\tprints usage information\n");
    printf("  -o, --ota\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -b, --no-baseband\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("  -e, --ecid ECID\tmanually specify ECID to be used for fetching blobs, instead of using random ones\n");
    printf("                 \tECID must be either dec or hex eg. 5482657301265 or ab46efcbf71\n");
    printf("      --beta\t\trequest ticket for beta instead of normal relase (use with -o)\n");
    printf("      --list-devices\tlist all known devices\n");
    printf("      --list-ios\tlist all known ios versions\n");
    printf("      --build-manifest\tmanually specify buildmanifest. (can be used with -d)\n");
    printf("      --nocache       \tignore caches and redownload required files\n");
    printf("      --print-tss-request\n");
    printf("      --print-tss-response\n");
    printf("\n");
}

int64_t parseECID(const char *ecid){
    const char *ecidBK = ecid;
    int isHex = 0;
    int64_t ret = 0;
    
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
                return -1; //ERROR parsing failed
            }
        }
    }
    
    return ret;
}

int main(int argc, const char * argv[]) {
    
    dbglog = 1;
    idevicerestore_debug = 0;
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    
    char *device = 0;
    char *ios = 0;
    char *buildmanifest = 0;
    char *ecid = 0;
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    while ((opt = getopt_long(argc, (char* const *)argv, "d:i:e:hbo", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
            case 'd': // long option: "device"; can be called as short option
                device = optarg;
                break;
            case 'i': // long option: "ios"; can be called as short option
                ios = optarg;
                break;
            case 'e': // long option: "ecid"; can be called as short option
                ecid = optarg;
                break;
            case 'b': // long option: "no-baseband"; can be called as short option
                flags |= FLAG_NO_BASEBAND;
                break;
            case 'o': // long option: "ota"; can be called as short option
                flags |= FLAG_OTA;
                break;
            case '1': // only long option: "list-devices"
                flags |= FLAG_LIST_DEVICES;
                break;
            case '2': // only long option: "list-ios"
                flags |= FLAG_LIST_IOS;
                break;
            case '3': // only long option: "build-manifest"
                flags |= FLAG_BUILDMANIFEST;
                buildmanifest = optarg;
                break;
            case '4': // only long option: "print-tss-request"
                print_tss_request = 1;
                break;
            case '5': // only long option: "print-tss-response"
                print_tss_response = 1;
                break;
            case '6': // only long option: "beta"
                flags |= FLAG_BETA;
                break;
            case '7': // only long option: "nocache"
                nocache = 1;
                break;
            default:
                cmd_help();
                return -1;
        }
    }
    int err = 0;
    int isSigned = 0;
    char *firmwareJson = NULL;
    jsmntok_t *firmwareTokens = NULL;
    int64_t ecidNum = 0;
#define reterror(code,a ...) {error(a); err = code; goto error;}
    
    if (ecid) {
        if ((ecidNum = parseECID(ecid)) <0){
            reterror(-7, "[TSSC] ERROR: manually specified ecid=%s, but parsing failed\n",ecid);
        }else{
            info("[TSSC] manually specified ecid to use, parsed \"%s\" to dec:%lld hex:%llx\n",ecid,ecidNum,ecidNum);
        }
    }
    
    
    
    firmwareJson = (flags & FLAG_OTA) ? getOtaJson() : getFirmwareJson();
    if (!firmwareJson) reterror(-6,"[TSSC] ERROR: could not get firmware.json\n");
    
    int cnt = parseTokens(firmwareJson, &firmwareTokens);
    if (cnt < 1) reterror(-2,"[TSSC] ERROR: parsing %s.json failed\n",(flags & FLAG_OTA) ? "ota" : "firmware");
    
    if (flags & FLAG_LIST_DEVICES) {
        printListOfDevices(firmwareJson, firmwareTokens);
    }else if (flags & FLAG_LIST_IOS){
        if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
        if (!checkDeviceExists(device, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-4,"[TSSC] ERROR: device %s could not be found in devicelist\n",device);
        
        printListOfiOSForDevice(firmwareJson, firmwareTokens, device, (flags & FLAG_OTA));
    }else{
        //request ticket
        if (buildmanifest) {
            if (device && !checkDeviceExists(device, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-4,"[TSSC] ERROR: device %s could not be found in devicelist\n",device);
            
            isSigned = isManifestSignedForDevice(buildmanifest, &device, ecidNum, !(flags & FLAG_NO_BASEBAND), &ios);

        }else{
            if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
            if (!ios) reterror(-5,"[TSSC] ERROR: please specify an iOS version for this option\n\tuse -h for more help\n");
            if (!checkFirmwareForDeviceExists(device, ios, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-6, "[TSSC] ERROR: either device %s does not exist, or there is no iOS %s for it.\n",device,ios);
            
            isSigned = isVersionSignedForDevice(firmwareJson, firmwareTokens, ios, device, ecidNum, (flags & FLAG_OTA), !(flags & FLAG_NO_BASEBAND), (flags & FLAG_BETA));
        }
        
        printf("\niOS %s for device %s %s being signed!\n",ios,device, (isSigned) ? "IS" : "IS NOT");
    }
    
    
    
error:
    if (firmwareJson) free(firmwareJson);
    if (firmwareTokens) free(firmwareTokens);
    return err ? err : isSigned;
#undef reterror
}

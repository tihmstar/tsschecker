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
#define FLAG_LIST_DEVIECS   1 << 1
#define FLAG_OTA            1 << 2
#define FLAG_NO_BASEBAND    1 << 3

int dbglog;
int idevicerestore_debug;
int print_tss_request;
int print_tss_response;

static struct option longopts[] = {
    { "list-devices",   no_argument,       NULL, 'l' },
    { "list-ios",       no_argument,       NULL, 'l' },
    { "device",         required_argument, NULL, 'd' },
    { "ios",            required_argument, NULL, 'i' },
    { "help",           no_argument,       NULL, 'h' },
    { "no-baseband",       no_argument,       NULL, 'b' },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    
    printf("  -d, --device MODEL\tspecific device by its MODEL (eg. iPhone4,1)\n");
    printf("  -i, --ios VERSION\tspecific iOS version (eg. 6.1.3)\n");
    printf("  -h, --help\t\tprints usage information\n");
    printf("  -o, --ota\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -b, --no-baseband\t\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-ios\t\tlist all known ios versions\n");
//    printf("  -a, --add\t\tadd connected devices to list of know devices\n");
//    printf("           \t\tthese devices' ECID will be used to check tss status instead of random values\n");
//    printf("           \t\tshsh blobs are saved for these devices in case the version is signed\n");
    printf("\n");
}

int main(int argc, const char * argv[]) {
    
    dbglog = 1;
    idevicerestore_debug = 0;
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    
    char *device = 0;
    char *ios = 0;
    
    if (argc == 1){
        cmd_help();
        return -1;
    }
    while ((opt = getopt_long(argc, (char* const *)argv, "hod:i:l", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h':
                cmd_help();
                return 0;
            case 'd':
                device = optarg;
                break;
            case 'i':
                ios = optarg;
                break;
            case 'b':
                flags |= FLAG_NO_BASEBAND;
                break;
            case 'o':
                flags |= FLAG_OTA;
                break;
            case 'l':
                for (int i=0; longopts[i].name != NULL; i++) {
                    int bb = 0;
                    for (int ii=0; ii<argc; ii++) {
                        if (strlen(argv[ii]) > 2 && strncmp(longopts[i].name, argv[ii] + 2, strlen(longopts[i].name)) == 0) {
                            flags |= (i == 0) ? FLAG_LIST_DEVIECS : FLAG_LIST_IOS;
                            bb = 1;
                            break;
                        }
                    }
                    if (bb) break;
                }
                break;
            default:
                cmd_help();
                return -1;
        }
    }
    int err = 0;
    char *firmwareJson = NULL;
    jsmntok_t *firmwareTokens = NULL;
#define reterror(code,a ...) {error(a); err = code; goto error;}
    
    firmwareJson = (flags & FLAG_OTA) ? getOtaJson() : getFirmwareJson();
    if (!firmwareJson) reterror(-6,"[TSSC] ERROR: could not get firmware.json\n");
    
    int cnt = parseTokens(firmwareJson, &firmwareTokens);
    if (cnt < 1) reterror(-2,"[TSSC] ERROR: parsing firmware.json failed\n");
    
    if (flags & FLAG_LIST_DEVIECS) {
        printListOfDevices(firmwareJson, firmwareTokens);
    }else if (flags & FLAG_LIST_IOS){
        if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
        if (!checkDeviceExists(device, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-4,"[TSSC] ERROR: device %s could not be found in devicelist\n",device);
        
        printListOfiOSForDevice(firmwareJson, firmwareTokens, device, (flags & FLAG_OTA));
    }else{
        //request ticket
        if (!device) reterror(-3,"[TSSC] ERROR: please specify a device for this option\n\tuse -h for more help\n");
        if (!ios) reterror(-5,"[TSSC] ERROR: please specify an iOS version for this option\n\tuse -h for more help\n");
        if (!checkFirmwareForDeviceExists(device, ios, firmwareJson, firmwareTokens, (flags & FLAG_OTA))) reterror(-6, "[TSSC] ERROR: either device %s does not exist, or there is no iOS %s for it.\n",device,ios);
        
        int isSigned = isVersionSignedForDevice(firmwareJson, firmwareTokens, ios, device, (flags & FLAG_OTA), !(flags & FLAG_NO_BASEBAND));
        printf("\niOS %s for device %s %s being signed!\n",ios,device, (isSigned) ? "IS" : "IS NOT");
    }
    
    
    
error:
    if (firmwareJson) free(firmwareJson);
    if (firmwareTokens) free(firmwareTokens);
    return err;
#undef reterror
}

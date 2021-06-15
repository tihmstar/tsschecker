//
//  tsschecker.c
//  tsschecker
//
//  Created by tihmstar on 07.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#include <stdio.h>

#include "tsschecker.h"
#include "debug.h"
#include "download.h"
#include "tss.h"

#include <libfragmentzip/libfragmentzip.h>
#include <libirecovery.h>

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA1(d, n, md) CC_SHA1(d, n, md)
#   define SHA384(d, n, md) CC_SHA384(d, n, md)
#else
#   include <openssl/sha.h>
#endif // __APPLE__

#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json/condensed"
#define FIRMWARE_OTA_JSON_URL "https://api.ipsw.me/v2.1/ota.json/condensed"

/* for KTRR devices this value - 32 */
#define NONCELEN_BASEBAND 20
#define NONCELEN_SEP      20

#define swapchar(a,b) ((a) ^= (b),(b) ^= (a),(a) ^= (b)) //swaps a and b, unless they are the same variable
#define printJString(str) printf("%.*s",(int)str->size,str->value)

#ifdef WIN32
#include <windows.h>
#define __mkdir(path, mode) mkdir(path)
static int win_path_didinit = 0;
static const char *win_paths[4];

enum paths{
    kWINPathTSSCHECKER,
    kWINPathOTA,
    kWINPathFIRMWARE
};

static const char *win_pathvars[]={
    "\\tsschecker",
    "\\ota.json",
    "\\firmwares.json"
};

static const char *win_path_get(enum paths path){
    if (!win_path_didinit) memset(win_paths, 0, sizeof(win_paths));
    if (win_paths[path]) return win_paths[path];
    
    const char *tmp = getenv("TMP");
    if (tmp && *tmp){
        size_t len = strlen(tmp) + strlen(win_pathvars[path]) + 1;
        win_paths[path] = (char *)malloc(len);
        memset((char*)win_paths[path], '\0', len);
        strcat((char*)win_paths[path], tmp);
        strcat((char*)win_paths[path], win_pathvars[path]);
        return win_paths[path];
    }
    
    error("DEBUG: tmp=%s win_paths[path]=%s\n",tmp,win_paths[path]);
    error("FATAL could not get TMP path. exiting!\n");
    exit(123);
    return NULL;
}

#define MANIFEST_SAVE_PATH win_path_get(kWINPathTSSCHECKER)
#define FIRMWARE_OTA_JSON_PATH win_path_get(kWINPathOTA)
#define FIRMWARE_JSON_PATH win_path_get(kWINPathFIRMWARE)
#define DIRECTORY_DELIMITER_STR "\\"
#define DIRECTORY_DELIMITER_CHR '\\'

#else

#define MANIFEST_SAVE_PATH "/tmp/tsschecker"
#define FIRMWARE_OTA_JSON_PATH "/tmp/ota.json"
#define FIRMWARE_JSON_PATH "/tmp/firmwares.json"
#define DIRECTORY_DELIMITER_STR "/"
#define DIRECTORY_DELIMITER_CHR '/'

#include <sys/stat.h>
#define __mkdir(path, mode) mkdir(path, mode)

#endif

#pragma mark getJson functions

int dbglog = 0;
int print_tss_request = 0;
int print_tss_response = 0;
int nocache = 0;
int save_shshblobs = 0;
const char *shshSavePath = "."DIRECTORY_DELIMITER_STR;

// iPhone & iPod touch (1st generations) do not use SHSH or APTicket.
static struct bbdevice bbdevices[] = {
    // Apple Silicon Macs
    {"ADP3,2", 0, 0},         // Developer Transition Kit
    {"iMac21,1", 0, 0},       // iMac (24-inch, M1, 2021)
    {"iMac21,2", 0, 0},       // iMac (24-inch, M1, 2021)
    {"Macmini9,1", 0, 0},     // Mac Mini (M1, 2020)
    {"MacBookAir10,1", 0, 0}, // MacBook Air (M1, 2020)
    {"MacBookPro17,1", 0, 0}, // MacBook Pro (13-inch, M1, 2020)
    
    // Apple T2 Coprocessor
    {"iBridge2,1", 0, 0}, // Apple T2 iMacPro1,1 (j137)
    {"iBridge2,3", 0, 0}, // Apple T2 MacBookPro15,1 (j680)
    {"iBridge2,4", 0, 0}, // Apple T2 MacBookPro15,2 (j132)
    {"iBridge2,5", 0, 0}, // Apple T2 Macmini8,1 (j174)
    {"iBridge2,6", 0, 0}, // Apple T2 MacPro7,1 (j160)
    {"iBridge2,7", 0, 0}, // Apple T2 MacBookPro15,3 (j780)
    {"iBridge2,8", 0, 0}, // Apple T2 MacBookAir8,1 (j140k)
    {"iBridge2,10", 0, 0}, // Apple T2 MacBookPro15,4 (j213)
    {"iBridge2,12", 0, 0}, // Apple T2 MacBookAir8,2 (j140a)
    {"iBridge2,14", 0, 0}, // Apple T2 MacBookPro16,1 (j152f)
    {"iBridge2,15", 0, 0}, // Apple T2 MacBookAir9,1 (j230k)
    {"iBridge2,16", 0, 0}, // Apple T2 MacBookPro16,2 (j214k)
    {"iBridge2,19", 0, 0}, // Apple T2 iMac20,1 (j185)
    {"iBridge2,20", 0, 0}, // Apple T2 iMac20,2 (j185f)
    {"iBridge2,21", 0, 0}, // Apple T2 MacBookPro16,3 (j223)
    {"iBridge2,22", 0, 0}, // Apple T2 MacBookPro16,4 (j215)
    
    // iPod touches
    {"iPod2,1", 0, 0}, // 2nd gen
    {"iPod3,1", 0, 0}, // 3rd gen
    {"iPod4,1", 0, 0}, // 4th gen
    {"iPod5,1", 0, 0}, // 5th gen
    {"iPod7,1", 0, 0}, // 6th gen
    {"iPod9,1", 0, 0}, // 7th gen
    
    // iPhones
    {"iPhone3,1",  257, 12},        // iPhone 4 GSM
    {"iPhone3,2",  257, 12},        // iPhone 4 GSM (2012, Rev A)
    {"iPhone3,3",  2, 4},           // iPhone 4 CDMA
    {"iPhone4,1",  2, 4},           // iPhone 4s
    {"iPhone5,1",  3255536192, 4},  // iPhone 5 (GSM)
    {"iPhone5,2",  3255536192, 4},  // iPhone 5 (Global)
    {"iPhone5,3",  3554301762, 4},  // iPhone 5c (GSM)
    {"iPhone5,4",  3554301762, 4},  // iPhone 5c (Global)
    {"iPhone6,1",  3554301762, 4},  // iPhone 5s (GSM)
    {"iPhone6,2",  3554301762, 4},  // iPhone 5s (Global)
    {"iPhone7,1",  3840149528, 4},  // iPhone 6 Plus
    {"iPhone7,2",  3840149528, 4},  // iPhone 6
    {"iPhone8,1",  3840149528, 4},  // iPhone 6s
    {"iPhone8,2",  3840149528, 4},  // iPhone 6s Plus
    {"iPhone8,4",  3840149528, 4},  // iPhone SE
    {"iPhone9,1",  2315222105, 4},  // iPhone 7 (Global)
    {"iPhone9,2",  2315222105, 4},  // iPhone 7 Plus (Global)
    {"iPhone9,3",  1421084145, 12}, // iPhone 7 GSM
    {"iPhone9,4",  1421084145, 12}, // iPhone 7 Plus (GSM)
    {"iPhone10,1", 2315222105, 4},  // iPhone 8 (Global)
    {"iPhone10,2", 2315222105, 4},  // iPhone 8 Plus (Global)
    {"iPhone10,3", 2315222105, 4},  // iPhone X (Global)
    {"iPhone10,4", 524245983, 12},  // iPhone 8 (GSM)
    {"iPhone10,5", 524245983, 12},  // iPhone 8 Plus (GSM)
    {"iPhone10,6", 524245983, 12},  // iPhone X GSM
    {"iPhone11,2", 165673526, 12},  // iPhone XS
    {"iPhone11,4", 165673526, 12},  // iPhone XS Max (China)
    {"iPhone11,6", 165673526, 12},  // iPhone XS Max (Global)
    {"iPhone11,8", 165673526, 12},  // iPhone XR
    {"iPhone12,1", 524245983, 12},  // iPhone 11
    {"iPhone12,3", 524245983, 12},  // iPhone 11 Pro
    {"iPhone12,5", 524245983, 12},  // iPhone 11 Pro Max
    {"iPhone12,8", 524245983, 12},  // iPhone SE (2020)
    {"iPhone13,1", 3095201109, 4},  // iPhone 12 mini
    {"iPhone13,2", 3095201109, 4},  // iPhone 12
    {"iPhone13,3", 3095201109, 4},  // iPhone 12 Pro
    {"iPhone13,4", 3095201109, 4},  // iPhone 12 Pro Max
    
    // iPads
    {"iPad1,1",  0, 0},          // iPad (1st gen)
    {"iPad2,1",  0, 0},          // iPad 2 Wi-Fi
    {"iPad2,2",  257, 12},       // iPad 2 GSM
    {"iPad2,3",  257, 12},       // iPad 2 CDMA
    {"iPad2,4",  0, 0},          // iPad 2 Wi-Fi (2012, Rev A)
    {"iPad3,1",  0, 0},          // iPad (3rd gen, Wi-Fi)
    {"iPad3,2",  4, 4},          // iPad (3rd gen, CDMA)
    {"iPad3,3",  4, 4},          // iPad (3rd gen, GSM)
    {"iPad3,4",  0, 0},          // iPad with Retina display (4th gen, Wi-Fi)
    {"iPad3,5",  3255536192, 4}, // iPad with Retina display (4th gen, CDMA)
    {"iPad3,6",  3255536192, 4}, // iPad with Retina display (4th gen, GSM)
    {"iPad6,11", 0, 0},          // iPad (5th gen, 2017, Wi-Fi)
    {"iPad6,12", 3840149528, 4}, // iPad (5th gen, 2017, Cellular)
    {"iPad7,5",  0, 0},          // iPad (6th gen, 2018, Wi-Fi)
    {"iPad7,6",  3840149528, 4}, // iPad (6th gen, 2018, Cellular)
    {"iPad7,11", 0, 0},          // iPad (7th gen, 2019, Wi-Fi)
    {"iPad7,12", 165673526, 12}, // iPad (7th gen, 2019, Cellular)
    {"iPad11,6", 0, 0},          // iPad (8th gen, 2020, Wi-Fi)
    {"iPad11,7", 165673526, 12}, // iPad (8th gen, 2020, Cellular)
    
    // iPad minis
    {"iPad2,5",  0, 0},          // iPad mini (1st gen, Wi-Fi)
    {"iPad2,6",  3255536192, 4}, // iPad mini (1st gen, CDMA)
    {"iPad2,7",  3255536192, 4}, // iPad mini (1st gen, GSM)
    {"iPad4,4",  0, 0},          // iPad mini 2 (Wi-Fi)
    {"iPad4,5",  3554301762, 4}, // iPad mini 2 (Cellular)
    {"iPad4,6",  3554301762, 4}, // iPad mini 2 (Cellular, China)
    {"iPad4,7",  0, 0},          // iPad mini 3 (Wi-Fi)
    {"iPad4,8",  3554301762, 4}, // iPad mini 3 (Cellular)
    {"iPad4,9",  3554301762, 4}, // iPad mini 3 (Cellular, China)
    {"iPad5,1",  0, 0},          // iPad mini 4 (Wi-Fi)
    {"iPad5,2",  3840149528, 4}, // iPad mini 4 (Cellular)
    {"iPad11,1", 0, 0},          // iPad mini (5th gen, Wi-Fi)
    {"iPad11,2", 165673526, 12}, // iPad mini (5th gen, Cellular)
    
    // iPad Airs
    {"iPad4,1",  0, 0},          // iPad Air (Wi-Fi)
    {"iPad4,2",  3554301762, 4}, // iPad Air (Cellular)
    {"iPad4,3",  3554301762, 4}, // iPad Air (Cellular, China)
    {"iPad5,3",  0, 0},          // iPad Air 2 (Wi-Fi)
    {"iPad5,4",  3840149528, 4}, // iPad Air 2 (Cellular)
    {"iPad11,3", 0, 0},          // iPad Air (3rd gen, Wi-Fi)
    {"iPad11,4", 165673526, 12}, // iPad Air (3rd gen, Cellular)
    {"iPad13,1", 0, 0},          // iPad Air (4th gen, Wi-Fi)
    {"iPad13,2", 524245983, 12}, // iPad Air (4th gen, Cellular)
    
    // iPad Pros
    {"iPad6,3",    0, 0},            // iPad Pro (9.7-inch, Wi-Fi)
    {"iPad6,4",    3840149528, 4},   // iPad Pro (9.7-inch, Cellular)
    {"iPad6,7",    0, 0},            // iPad Pro (12.9-inch, 1st gen, Wi-Fi)
    {"iPad6,8",    3840149528, 4},   // iPad Pro (12.9-inch, 1st gen, Cellular)
    {"iPad7,1",    0, 0},            // iPad Pro (12.9-inch, 2nd gen, Wi-Fi)
    {"iPad7,2",    2315222105, 4},   // iPad Pro (12.9-inch, 2nd gen, Cellular)
    {"iPad7,3",    0, 0},            // iPad Pro (10.5-inch, Wi-Fi)
    {"iPad7,4",    2315222105, 4},   // iPad Pro (10.5-inch, Cellular)
    {"iPad8,1",    0, 0},            // iPad Pro (11-inch, 1st gen, Wi-Fi)
    {"iPad8,2",    0, 0},            // iPad Pro (11-inch, 1st gen, 1TB, Wi-Fi)
    {"iPad8,3",    165673526, 12},   // iPad Pro (11-inch, 1st gen, Cellular)
    {"iPad8,4",    165673526, 12},   // iPad Pro (11-inch, 1st gen, 1TB, Cellular)
    {"iPad8,5",    0, 0},            // iPad Pro (12.9-inch, 3rd gen, Wi-Fi)
    {"iPad8,6",    0, 0},            // iPad Pro (12.9-inch, 3rd gen, 1TB, Wi-Fi)
    {"iPad8,7",    165673526, 12},   // iPad Pro (12.9-inch, 3rd gen, Cellular)
    {"iPad8,8",    165673526, 12},   // iPad Pro (12.9-inch, 3rd gen, 1TB, Cellular)
    {"iPad8,9",    0, 0},            // iPad Pro (11-inch, 2nd gen, Wi-Fi)
    {"iPad8,10",   524245983, 12},   // iPad Pro (11-inch, 2nd gen, Cellular)
    {"iPad8,11",   0, 0},            // iPad Pro (12.9-inch, 4th gen, Wi-Fi)
    {"iPad8,12",   524245983, 12},   // iPad Pro (12.9-inch, 4th gen, Cellular)
    {"iPad13,4",   0, 0},            // iPad Pro (11-inch, 3rd gen, Wi-Fi)
    {"iPad13,5",   0, 0},            // iPad Pro (11-inch, 3rd gen, 2TB, Wi-Fi)
    {"iPad13,6",   3095201109, 4},   // iPad Pro (11-inch, 3rd gen, Cellular)
    {"iPad13,7",   3095201109, 4},   // iPad Pro (11-inch, 3rd gen, 2TB, Cellular)
    {"iPad13,8",   0, 0},            // iPad Pro (12.9-inch, 5th gen, Wi-Fi)
    {"iPad13,9",   0, 0},            // iPad Pro (12.9-inch, 5th gen, 2TB, Wi-Fi)
    {"iPad13,10",  3095201109, 4},   // iPad Pro (12.9-inch, 5th gen, Cellular)
    {"iPad13,11",  3095201109, 4},   // iPad Pro (12.9-inch, 5th gen, 2TB, Cellular)
    
    // Apple Watches
    {"Watch1,1",  0, 0},          // Apple Watch 1st gen (38mm)
    {"Watch1,2",  0, 0},          // Apple Watch 1st gen (42mm)
    {"Watch2,6",  0, 0},          // Apple Watch Series 1 (38mm)
    {"Watch2,7",  0, 0},          // Apple Watch Series 1 (42mm)
    {"Watch2,3",  0, 0},          // Apple Watch Series 2 (38mm)
    {"Watch2,4",  0, 0},          // Apple Watch Series 2 (42mm)
    {"Watch3,1",  3840149528, 4}, // Apple Watch Series 3 (38mm GPS + Cellular)
    {"Watch3,2",  3840149528, 4}, // Apple Watch Series 3 (42mm GPS + Cellular)
    {"Watch3,3",  0, 0},          // Apple Watch Series 3 (38mm GPS)
    {"Watch3,4",  0, 0},          // Apple Watch Series 3 (42mm GPS)
    {"Watch4,1",  0, 0},          // Apple Watch Series 4 (40mm GPS)
    {"Watch4,2",  0, 0},          // Apple Watch Series 4 (44mm GPS)
    {"Watch4,3",  744114402, 12}, // Apple Watch Series 4 (40mm GPS + Cellular)
    {"Watch4,4",  744114402, 12}, // Apple Watch Series 4 (44mm GPS + Cellular)
    {"Watch5,1",  0, 0},          // Apple Watch Series 5 (40mm GPS)
    {"Watch5,2",  0, 0},          // Apple Watch Series 5 (44mm GPS)
    {"Watch5,3",  744114402, 12}, // Apple Watch Series 5 (40mm GPS + Cellular)
    {"Watch5,4",  744114402, 12}, // Apple Watch Series 5 (44mm GPS + Cellular)
    {"Watch5,9",  0, 0},          // Apple Watch SE (40mm GPS)
    {"Watch5,10", 0, 0},          // Apple Watch SE (44mm GPS)
    {"Watch5,11", 744114402, 12}, // Apple Watch SE (40mm GPS + Cellular)
    {"Watch5,12", 744114402, 12}, // Apple Watch SE (44mm GPS + Cellular)
    {"Watch6,1",  0, 0},          // Apple Watch Series 6 (40mm GPS)
    {"Watch6,2",  0, 0},          // Apple Watch Series 6 (44mm GPS)
    {"Watch6,3",  744114402, 12}, // Apple Watch Series 6 (40mm GPS + Cellular)
    {"Watch6,4",  744114402, 12}, // Apple Watch Series 6 (44mm GPS + Cellular)
    
    // HomePods
    {"AudioAccessory1,1", 0, 0}, // HomePod 1st gen
    {"AudioAccessory1,2", 0, 0}, // HomePod 1st gen (2018)
    {"AudioAccessory5,1", 0, 0}, // HomePod mini
    
    // Apple TVs
    {"AppleTV1,1",  0, 0}, // 1st gen
    {"AppleTV2,1",  0, 0}, // 2nd gen
    {"AppleTV3,1",  0, 0}, // 3rd gen
    {"AppleTV3,2",  0, 0}, // 3rd gen (2013)
    {"AppleTV5,3",  0, 0}, // 4th gen
    {"AppleTV6,2",  0, 0}, // 4K
    {"AppleTV11,1", 0, 0}, // 4K 2nd gen
    {NULL, 0, 0}
};

inline static t_bbdevice bbdevices_get_all() {
    return bbdevices;
}

char *getFirmwareJson(){
    info("[TSSC] opening firmwares.json\n");
    FILE *f = fopen(FIRMWARE_JSON_PATH, "rb");
    if (!f || nocache){
        downloadFile(FIRMWARE_JSON_URL, FIRMWARE_JSON_PATH);
        f = fopen(FIRMWARE_JSON_PATH, "rb");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *fJson = malloc(fsize + 1);
    fread(fJson, fsize, 1, f);
    fclose(f);
    return fJson;
}

char *getOtaJson(){
    info("[TSSC] opening ota.json\n");
    FILE *f = fopen(FIRMWARE_OTA_JSON_PATH, "rb");
    if (!f || nocache){
        downloadFile(FIRMWARE_OTA_JSON_URL, FIRMWARE_OTA_JSON_PATH);
        f = fopen(FIRMWARE_OTA_JSON_PATH, "rb");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *fJson = malloc(fsize + 1);
    fread(fJson, fsize, 1, f);
    fclose(f);
    return fJson;
}

#pragma mark more get functions
const char *getBoardconfigFromModel(const char *model){
    const char *rt = NULL;
    irecv_device_t table = irecv_devices_get_all();
    //iterate through table until find correct entry
    //table is terminated with {NULL, NULL, -1, -1} entry, return that if device not found
    while (table->product_type){
        if (strcasecmp(model, table->product_type) == 0){
            if (rt){
                warning("can't unambiguously map model to boardconfig for device %s\n",model);
                return NULL;
            }else
                rt = table->hardware_model;
        }
        
        table++;
    }
    
    return rt;
}

const char *getModelFromBoardconfig(const char *boardconfig){
    const char *rt = NULL;
    irecv_device_t table = irecv_devices_get_all();
    //iterate through table until find correct entry
    //table is terminated with {NULL, NULL, -1, -1} entry, return that if device not found
    while (table->product_type){
        if (strcasecmp(boardconfig, table->hardware_model) == 0){
            if (rt){
                warning("can't unambiguously map boardconfig to model for device %s\n",boardconfig);
                return NULL;
            }else
                rt = table->product_type;
        }
        
        table++;
    }
    
    return rt;
}

plist_t getBuildidentityWithBoardconfig(plist_t buildManifest, const char *boardconfig, int isUpdateInstall){
    plist_t selected_build_identity = NULL;
    plist_t buildidentities = plist_dict_get_item(buildManifest, "BuildIdentities");
    if (!buildidentities || plist_get_node_type(buildidentities) != PLIST_ARRAY){
        error("[TSSR] Error: could not get BuildIdentities\n");
        return NULL;
    }
    
    for (int i = 0; i < plist_array_get_size(buildidentities); i++) {
        info("[TSSR] Checking BuildIdentity %d\n", i);
        plist_t build_identity = plist_array_get_item(buildidentities, i);
        if (!build_identity || plist_get_node_type(build_identity) != PLIST_DICT){
            warning("[TSSR] Could not get BuildIdentity\n");
            continue;
        }
        plist_t infodict = plist_dict_get_item(build_identity, "Info");
        if (!infodict || plist_get_node_type(infodict) != PLIST_DICT){
            warning("[TSSR] Could not get info dictionary\n");
            continue;
        }
        plist_t RestoreBehavior = plist_dict_get_item(infodict, "RestoreBehavior");
        // certain buildidentities (eg: Research Developer Erase Install) in some manifests
        // don't contain a RestoreBehavior
        if (!RestoreBehavior || plist_get_node_type(RestoreBehavior) != PLIST_STRING){
            warning("[TSSR] Could not get RestoreBehavior\n");
            continue;
        }
        char *string = NULL;
        plist_get_string_val(RestoreBehavior, &string);
        /* assuming there are only Erase and Update. If it's not Erase it must be Update
           also converting isUpdateInstall to bool (1 or 0) */
        if ((strncmp(string, "Erase", strlen(string)) != 0) == !isUpdateInstall){
            /* continue when Erase found but isUpdateInstall
               is true or Update found and isUpdateInstall is false */
            continue;
        }
        
        plist_t DeviceClass = plist_dict_get_item(infodict, "DeviceClass");
        if (!DeviceClass || plist_get_node_type(DeviceClass) != PLIST_STRING){
            warning("[TSSR] Could not get DeviceClass\n");
        }
        plist_get_string_val(DeviceClass, &string);
        if (strcasecmp(string, boardconfig) == 0)
        {
            info("[TSSR] Selected BuildIdentity for request\n");
            selected_build_identity = build_identity;
            break;
        }
    }
    return selected_build_identity;
}

plist_t getBuildidentity(plist_t buildManifest, const char *model, int isUpdateInstall){
    plist_t rt = NULL;
#define reterror(a ... ) {error(a); rt = NULL; goto error;}
    
    const char *boardconfig = getBoardconfigFromModel(model);
    if (!boardconfig)
        reterror("[TSSR] can't find boardconfig for device=%s please manually use --boardconfig\n",model);
    
    rt = getBuildidentityWithBoardconfig(buildManifest, boardconfig, isUpdateInstall);
    
error:
    return rt;
#undef reterror
}

#pragma mark json functions
long parseTokens(const char *json, jssytok_t **tokens){
    
    log("[JSON] counting elements\n");
    long tokensCnt = jssy_parse(json, strlen(json), NULL, 0);
    *tokens = (jssytok_t*)malloc(sizeof(jssytok_t) * tokensCnt);
    
    log("[JSON] parsing elements\n");
    return jssy_parse(json, strlen(json), *tokens, sizeof(jssytok_t) * tokensCnt);
}

#pragma mark get functions
//returns NULL terminated array of t_versionURL objects
t_versionURL *getFirmwareUrls(const char *deviceModel, t_iosVersion *versVals, jssytok_t *tokens){
    t_versionURL *rets = NULL;
    const t_versionURL *rets_base = NULL;
    unsigned retcounter = 0;
    
    jssytok_t *firmwares = getFirmwaresForDevice(deviceModel, tokens, versVals->isOta);
    const char *versstring = (versVals->buildID) ? versVals->buildID : versVals->version;
    
    if (!firmwares)
        return error("[TSSC] device '%s' could not be found in devicelist\n", deviceModel), NULL;
    
malloc_rets:
    if (retcounter)
        memset(rets = (t_versionURL*)malloc(sizeof(t_versionURL)*(retcounter+1)), 0, sizeof(t_versionURL)*(retcounter+1));
    rets_base = rets;
    
    jssytok_t *tmp = firmwares->subval;
    for (size_t i=0; i<firmwares->size; tmp=tmp->next, i++) {
        jssytok_t *ios = jssy_dictGetValueForKey(tmp, (versVals->buildID) ? "buildid" : "version");
        
        if (ios->size == strlen(versstring) && strncmp(versstring, ios->value, ios->size) == 0) {
            
            if (versVals->isOta) {
                jssytok_t *releaseType = NULL;
                if (versVals->useBeta && !(releaseType = jssy_dictGetValueForKey(tmp, "releasetype"))) continue;
                else if (!versVals->useBeta);
                else if (strncmp(releaseType->value, "Beta", releaseType->size) != 0) continue;
            }
            
            jssytok_t *url = jssy_dictGetValueForKey(tmp, "url");
            jssytok_t *i_vers = jssy_dictGetValueForKey(tmp, "version");
            jssytok_t *i_build = jssy_dictGetValueForKey(tmp, "buildid");
            
            if (!versVals->version){
                versVals->version = (char*)malloc(i_vers->size+1);
                memcpy(versVals->version, i_vers->value, i_vers->size);
                versVals->version[i_vers->size] = '\0';
            }
            
            if (!rets) retcounter++;
            else{
                for (int i=0; rets_base[i].buildID; i++) {
                    if (strncmp(rets_base[i].buildID, i_build->value, i_build->size) == 0){
                        info("[TSSC] Marking duplicated buildid %s\n",rets_base[i].buildID);
                        rets->isDupulicate = 1;
                        break;
                    }
                }
                
                info("[TSSC] got firmwareurl for iOS %.*s build %.*s\n",(int)i_vers->size, i_vers->value,(int)i_build->size, i_build->value);
                rets->version = (char*)malloc(i_vers->size+1);
                memcpy(rets->version, i_vers->value, i_vers->size);
                rets->version[i_vers->size] = '\0';
                
                rets->buildID = (char*)malloc(i_build->size+1);
                memcpy(rets->buildID, i_build->value, i_build->size);
                rets->buildID[i_build->size] = '\0';
                
                rets->url = (char*)malloc(url->size+1);
                memcpy(rets->url, url->value, url->size);
                rets->url[url->size] = '\0';
                rets++;
            }
        }
    }
    
    if (!retcounter) return NULL;
    else if (!rets) goto malloc_rets;
    
    return (t_versionURL*)rets_base;
}

static void fragmentzip_callback(){}

int downloadPartialzip(const char *url, const char *file, const char *dst){
    log("[LFZP] downloading %s from %s\n",file,url);
    fragmentzip_t *info = fragmentzip_open(url);
    if (!info) {
        error("[LFZP] failed to open url\n");
        return -1;
    }
    int ret = fragmentzip_download_file(info, file, dst, fragmentzip_callback);
    if (ret){
        error("[LFZP] failed to download file (%d)\n",ret);
    }
    fragmentzip_close(info);
    return ret;
}

char *getBuildManifest(char *url, const char *device, const char *version, const char *buildID, int isOta){
    struct stat st = {0};
    
    size_t len = strlen(MANIFEST_SAVE_PATH) + strlen("/__") + strlen(device) + strlen(version) +1;
    if (buildID) len += strlen(buildID);
    if (isOta) len += strlen("ota");
    char *fileDir = malloc(len);
    memset(fileDir, 0, len);
    
    strcat(fileDir, MANIFEST_SAVE_PATH);
    strcat(fileDir, DIRECTORY_DELIMITER_STR);
    strcat(fileDir, device);
    strcat(fileDir, "_");
    strcat(fileDir, version);
    if (buildID){
        strcat(fileDir, "_");
        strcat(fileDir, buildID);
    }
    
    if (isOta) strcat(fileDir, "ota");
    
    memset(&st, 0, sizeof(st));
    if (stat(MANIFEST_SAVE_PATH, &st) == -1) __mkdir(MANIFEST_SAVE_PATH, 0700);
    
    //gen name
    char *name = fileDir + strlen(fileDir);
    while (*--name != DIRECTORY_DELIMITER_CHR);
    name ++;
    
    //get file
    FILE *f = fopen(fileDir, "rb");
    if (!url) {
        if (!f || nocache) return NULL;
        info("[TSSC] using cached Buildmanifest for %s\n",name);
    }else info("[TSSC] opening Buildmanifest for %s\n",name);
    
    if (!f || nocache){
        //download if it isn't there
        int got_buildmanifest = 0;

        if (!isOta) {
            int index = 0;
            for (int i = 0; i < strlen(url); i++) {
                if (url[i] == '/') {
                    index = i;
                }
            }

            char *buildmanifest_url = malloc(strlen(url));
            buildmanifest_url = strncpy(buildmanifest_url, url, index);
            buildmanifest_url[index] = '\0';
            buildmanifest_url = strcat(buildmanifest_url, "/BuildManifest.plist");

            if (downloadFile(buildmanifest_url, fileDir) == 0) {
                free(buildmanifest_url);
                got_buildmanifest = 1;
            }
        }

        if (!got_buildmanifest) {
            if (downloadPartialzip(url, (isOta) ? "AssetData/boot/BuildManifest.plist" : "BuildManifest.plist", fileDir)){
                free(fileDir);
                return NULL;
            }
        }

        f = fopen(fileDir, "rb");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    //load it
    char *buildmanifest = malloc(fsize + 1);
    fread(buildmanifest, fsize, 1, f);
    buildmanifest[fsize] = '\0';
    fclose(f);
    
    free(fileDir);
    return buildmanifest;
}

t_bbdevice getBBDeviceInfo(const char *deviceModel){
    t_bbdevice bbdevs = bbdevices_get_all();
    
    while (bbdevs->deviceModel && strcasecmp(bbdevs->deviceModel, deviceModel) != 0)
        bbdevs++;

    return bbdevs;
}

void debug_plist(plist_t plist);

void getRandNum(char *dst, size_t size, int base){
    srand((unsigned int)time(NULL));
    for (int i=0; i<size; i++) {
        int j;
        if (base == 256) dst[i] = rand() % base;
        else dst[i] = ((j = rand() % base) < 10) ? '0' + j : 'a' + j-10;
    }
}

#pragma mark tss functions
int tss_populate_devicevals(plist_t tssreq, uint64_t ecid, char *nonce, size_t nonce_size, char *sep_nonce, size_t sep_nonce_size, int image4supported){
    plist_dict_set_item(tssreq, "ApECID", plist_new_uint(ecid)); //0000000000000000
    if (nonce) {
        plist_dict_set_item(tssreq, "ApNonce", plist_new_data(nonce, nonce_size));//aa aa aa aa bb cc dd ee ff 00 11 22 33 44 55 66 77 88 99 aa
    }
    
    if (sep_nonce) {//aa aa aa aa bb cc dd ee ff 00 11 22 33 44 55 66 77 88 99 aa
        plist_dict_set_item(tssreq, "ApSepNonce", plist_new_data(sep_nonce, sep_nonce_size));
    }
    
    plist_dict_set_item(tssreq, "ApProductionMode", plist_new_bool(1));
    
    if (image4supported) {
        plist_dict_set_item(tssreq, "ApSecurityMode", plist_new_bool(1));
        plist_dict_set_item(tssreq, "ApSupportsImg4", plist_new_bool(1));
    } else {
        plist_dict_set_item(tssreq, "ApSupportsImg4", plist_new_bool(0));
    }
    
    return 0;
}

int tss_populate_basebandvals(plist_t tssreq, plist_t tssparameters, int64_t BbGoldCertId, uint8_t *BbSNUM, size_t bbsnumSize){
#define reterror(a...) {error(a); ret = -1; goto error;}
    int ret = 0;
    plist_t parameters = plist_copy(tssparameters);
    char bbnonce[NONCELEN_BASEBAND+1];
    
    int did_malloc_bbsnum = 0;
    if (!BbSNUM) {
        BbSNUM = (uint8_t *)malloc(bbsnumSize);
        getRandNum((char *)BbSNUM, bbsnumSize, 256);
        did_malloc_bbsnum = 1;
    }
    
    int64_t BbChipID = 0;
    
    getRandNum(bbnonce, NONCELEN_BASEBAND, 256);
    srand((unsigned int)time(NULL));
    int n=0; for (int i=1; i<7; i++) BbChipID += (rand() % 10) * pow(10, ++n);
    
    /* BasebandNonce not required */
//  plist_dict_set_item(parameters, "BbNonce", plist_new_data(bbnonce, noncelen));
//  plist_dict_set_item(parameters, "BbChipID", plist_new_uint(BbChipID));
    plist_dict_set_item(parameters, "BbGoldCertId", plist_new_uint(BbGoldCertId));
    plist_dict_set_item(parameters, "BbSNUM", plist_new_data((char *)BbSNUM, bbsnumSize));
    
    /* BasebandFirmware */
    if (tss_request_add_baseband_tags(tssreq, parameters, NULL) < 0) {
        reterror("[TSSR] failed to add baseband tags to TSS request\n");
    }
    
error:
    if (did_malloc_bbsnum) {
        free(BbSNUM);
    }
    return ret;
#undef reterror
}

int parseHex(const char *nonce, size_t *parsedLen, char *ret, size_t *retSize){
    size_t nonceLen = strlen(nonce);
    nonceLen = nonceLen/2 + nonceLen%2; //one byte more if len is odd
    
    if (retSize) *retSize = (nonceLen+1)*sizeof(char);
    if (!ret) return 0;
    
    memset(ret, 0, nonceLen+1);
    unsigned int nlen = 0;
    
    int next = strlen(nonce)%2 == 0;
    char tmp = 0;
    while (*nonce) {
        char c = *(nonce++);
        
        tmp *=16;
        if (c >= '0' && c<='9') {
            tmp += c - '0';
        }else if (c >= 'a' && c <= 'f'){
            tmp += 10 + c - 'a';
        }else if (c >= 'A' && c <= 'F'){
            tmp += 10 + c - 'A';
        }else{
            return -1; //ERROR parsing failed
        }
        if ((next =! next) && nlen < nonceLen) (void)(ret[nlen++] = tmp),tmp=0;
    }
    
    if (parsedLen) *parsedLen = nlen;
    return 0;
}

int tss_populate_random(plist_t tssreq, int is64bit, t_devicevals *devVals){
    size_t nonceLen = 32; //valid for all devices with KTRR
    if (!devVals->deviceModel)
        return error("[TSSR] internal error: devVals->deviceModel is missing\n"),-1;

    if (strncasecmp(devVals->deviceModel, "AudioAccessory1,", strlen("AudioAccessory1,")) == 0 ||
            strncasecmp(devVals->deviceModel, "AppleTV2,", strlen("AppleTV2,")) == 0 ||
            strncasecmp(devVals->deviceModel, "AppleTV3,", strlen("AppleTV3,")) == 0 ||
            strncasecmp(devVals->deviceModel, "AppleTV5,", strlen("AppleTV5,")) == 0 ||
            strncasecmp(devVals->deviceModel, "Watch1,", strlen("Watch1,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad1,", strlen("iPad1,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad2,", strlen("iPad2,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad3,", strlen("iPad3,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad4,", strlen("iPad4,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad5,", strlen("iPad5,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPad6,", strlen("iPad6,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone1,", strlen("iPhone1,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone2,", strlen("iPhone2,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone3,", strlen("iPhone3,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone4,", strlen("iPhone4,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone5,", strlen("iPhone5,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone6,", strlen("iPhone6,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone7,", strlen("iPhone7,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPhone8,", strlen("iPhone8,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPod2,", strlen("iPod2,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPod3,", strlen("iPod3,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPod4,", strlen("iPod4,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPod5,", strlen("iPod5,")) == 0 ||
            strncasecmp(devVals->deviceModel, "iPod7,", strlen("iPod7,")) == 0 )
        nonceLen = 20; //valid for devices without KTRR

    int n=0;
    srand((unsigned int)time(NULL));
    if (!devVals->ecid) for (int i=0; i<16; i++) devVals->ecid += (rand() % 10) * pow(10, n++);

    if (devVals->apnonce){
        if (!devVals->parsedApnonceLen)
            devVals->apnonce = NULL;
        else if (devVals->parsedApnonceLen != nonceLen)
            return error("[TSSR] parsed APNoncelen != requiredAPNoncelen (%u != %u)\n",(unsigned int)devVals->parsedApnonceLen,(unsigned int)nonceLen),-1;
    }else{
        devVals->apnonce = (char*)malloc((devVals->parsedApnonceLen = nonceLen)+1);
        //nonce is derived from generator with SHA1
        if (nonceLen == 20) {
            unsigned char zz[9] = {0};
            
            for (int i=0; i<sizeof(devVals->generator)-1; i++) {
                if (!devVals->generator[i]) goto makegen;
            }
            devVals->generator[sizeof(devVals->generator)-1] = 0;
            parseHex(devVals->generator+2, NULL, (char*)zz, NULL);
            swapchar(zz[0], zz[7]);
            swapchar(zz[1], zz[6]);
            swapchar(zz[2], zz[5]);
            swapchar(zz[3], zz[4]);
            goto makesha1;
        makegen:
            getRandNum((char*)zz, 8, 256);
            snprintf(devVals->generator, 19, "0x%02x%02x%02x%02x%02x%02x%02x%02x",zz[7],zz[6],zz[5],zz[4],zz[3],zz[2],zz[1],zz[0]);
        makesha1:
            SHA1(zz, 8, (unsigned char*)devVals->apnonce);
        }else if (nonceLen == 32){
            unsigned char zz[9] = {0};
            unsigned char genHash[48]; //SHA384 digest length
            
            for (int i=0; i<sizeof(devVals->generator)-1; i++) {
                if (!devVals->generator[i]) goto makegen2;
            }
            devVals->generator[sizeof(devVals->generator)-1] = 0;
            parseHex(devVals->generator+2, NULL, (char*)zz, NULL);
            swapchar(zz[0], zz[7]);
            swapchar(zz[1], zz[6]);
            swapchar(zz[2], zz[5]);
            swapchar(zz[3], zz[4]);
            goto makesha384;
        makegen2:
            getRandNum((char*)zz, 8, 256);
            snprintf(devVals->generator, 19, "0x%02x%02x%02x%02x%02x%02x%02x%02x",zz[7],zz[6],zz[5],zz[4],zz[3],zz[2],zz[1],zz[0]);
        makesha384:
            SHA384(zz, 8, genHash);
            memcpy(devVals->apnonce, genHash, 32);
        }else{
            return error("[TSSR] Automatic generator->nonce calculation failed. Unknown device with noncelen=%u\n",(unsigned int)nonceLen),-1;
        }
    }
    
    if (devVals->sepnonce){
        if (devVals->parsedSepnonceLen != NONCELEN_SEP)
            return error("[TSSR] parsed SEPNoncelen != requiredSEPNoncelen (%u != %u)",(unsigned int)devVals->parsedSepnonceLen,(unsigned int)NONCELEN_SEP),-1;
    }else{
        devVals->sepnonce = (char*)malloc((devVals->parsedSepnonceLen = NONCELEN_SEP) +1);
        getRandNum(devVals->sepnonce, devVals->parsedSepnonceLen, 256);
    }
    
    if (devVals->apnonce) devVals->apnonce[nonceLen] = '\0';
    devVals->sepnonce[NONCELEN_SEP] = '\0';
    
    debug("[TSSR] ecid=%llu\n",devVals->ecid);
    debug("[TSSR] ApNonce=%s\n",devVals->apnonce);
    debug("[TSSR] SepNonce=%s\n",devVals->sepnonce);
    
    int rt = tss_populate_devicevals(tssreq, devVals->ecid, devVals->apnonce, devVals->parsedApnonceLen, devVals->sepnonce, devVals->parsedSepnonceLen, is64bit);
    return rt;
}

int tssrequest(plist_t *tssreqret, char *buildManifest, t_devicevals *devVals, t_basebandMode basebandMode){
#define reterror(a...) {error(a); error = -1; goto error;}
    int error = 0;
    plist_t manifest = NULL;
    plist_t tssparameter = plist_new_dict();
    plist_t tssreq = tss_request_new(NULL);
    plist_t id0 = NULL;
    plist_from_xml(buildManifest, (unsigned)strlen(buildManifest), &manifest);
    
getID0:
    id0 = (devVals->deviceBoard)
                ? getBuildidentityWithBoardconfig(manifest, devVals->deviceBoard, devVals->isUpdateInstall)
                : getBuildidentity(manifest, devVals->deviceModel, devVals->isUpdateInstall);
    if (!id0 && !devVals->installType){
        warning("[TSSC] could not get BuildIdentity for installType=Erase. Using fallback installType=Update since user did not specify installType manually\n");

        devVals->installType = kInstallTypeUpdate;
        goto getID0;
    }
    
    if (!id0 || plist_get_node_type(id0) != PLIST_DICT){
        reterror("[TSSR] Error: could not get BuildIdentity for installType=%s\n",devVals->isUpdateInstall ? "Update" : "Erase");
    }
    plist_t manifestdict = plist_dict_get_item(id0, "Manifest");
    if (!manifestdict || plist_get_node_type(manifestdict) != PLIST_DICT){
        reterror("[TSSR] Error: could not get manifest\n");
    }
    plist_t sep = plist_dict_get_item(manifestdict, "SEP");
    int is64Bit = !(!sep || plist_get_node_type(sep) != PLIST_DICT);
    
    if (tss_populate_random(tssparameter,is64Bit,devVals))
        reterror("[TSSR] failed to populate tss request\n");
    
    tss_parameters_add_from_manifest(tssparameter, id0);
    if (tss_request_add_common_tags(tssreq, tssparameter, NULL) < 0) {
        reterror("[TSSR] ERROR: Unable to add common tags to TSS request\n");
    }
    
    if (tss_request_add_ap_tags(tssreq, tssparameter, NULL) < 0) {
        reterror("[TSSR] ERROR: Unable to add common tags to TSS request\n");
    }
    
    if (is64Bit) {
        if (tss_request_add_ap_img4_tags(tssreq, tssparameter) < 0) {
            reterror("[TSSR] ERROR: Unable to add img4 tags to TSS request\n");
        }
    } else {
        if (tss_request_add_ap_img3_tags(tssreq, tssparameter) < 0) {
            reterror("[TSSR] ERROR: Unable to add img3 tags to TSS request\n");
        }
    }
    if (basebandMode == kBasebandModeOnlyBaseband) {
        if (plist_dict_get_item(tssreq, "@ApImg4Ticket"))
            plist_dict_set_item(tssreq, "@ApImg4Ticket", plist_new_bool(0));
        if (plist_dict_get_item(tssreq, "@APTicket"))
            plist_dict_set_item(tssreq, "@APTicket", plist_new_bool(0));
        //TODO: don't use .shsh2 ending and don't save generator when saving only baseband
        info("[TSSR] User specified to request only a Baseband ticket.\n");
    }
    
    if (basebandMode != kBasebandModeWithoutBaseband) {
        //TODO: verify that this being int64_t instead of uint64_t doesn't actually break something
        t_bbdevice bbinfo = getBBDeviceInfo(devVals->deviceModel);
        int64_t BbGoldCertId = devVals->bbgcid ? devVals->bbgcid : bbinfo->bbgcid;
        size_t bbsnumSize = devVals->bbsnumSize ? devVals->bbsnumSize : bbinfo->bbsnumSize;
        if (BbGoldCertId != bbinfo->bbgcid || bbsnumSize != bbinfo->bbsnumSize) {
            info("\n[TSSR] Found undocumented baseband\n\n",
                 devVals->deviceBoard, devVals->deviceModel, BbGoldCertId, bbsnumSize);
        }
        
        if (BbGoldCertId == -1) {
            if (basebandMode == kBasebandModeOnlyBaseband){
                reterror("[TSSR] failed to get BasebandGoldCertID, but requested to get only baseband ticket. Aborting here!\n");
            }
            warning("[TSSR] there was an error getting BasebandGoldCertID, continuing without requesting Baseband ticket\n");
        }else if (BbGoldCertId) {
            if (tss_populate_basebandvals(tssreq, tssparameter, BbGoldCertId, devVals->bbsnum, bbsnumSize) < 0) {
                reterror("[TSSR] failed to populate baseband values\n");
            }
        }else{
            log("[TSSR] LOG: device %s doesn't need a baseband ticket, continuing without requesting a Baseband ticket\n",devVals->deviceModel);
        }
    }else{
        info("[TSSR] User specified to not request a baseband ticket.\n");
    }
    
    *tssreqret = tssreq;
error:
    if (manifest) plist_free(manifest);
    if (tssparameter) plist_free(tssparameter);
    if (error) (void)(plist_free(tssreq)), *tssreqret = NULL;
    return error;
#undef reterror
}

int isManifestBufSignedForDevice(char *buildManifestBuffer, t_devicevals *devVals, t_basebandMode basebandMode, const char* server_url_string){
#define reterror(a ...) {error(a); isSigned = -1; goto error;}
    int isSigned = 0;
    plist_t tssreq = NULL;
    plist_t apticket = NULL;
    plist_t apticket2 = NULL;
    plist_t apticket3 = NULL;
    
    if (tssrequest(&tssreq, buildManifestBuffer, devVals, basebandMode))
        reterror("[TSSR] failed to build tss request\n");

    isSigned = ((apticket = tss_request_send(tssreq, server_url_string)) > 0);
    
    if (print_tss_response) debug_plist(apticket);
    if (isSigned && save_shshblobs){
        if (!devVals->installType){
            plist_t tssreq2 = NULL;
            info("also requesting APTicket for installType=Update\n");
            devVals->installType = kInstallTypeUpdate;
            if (tssrequest(&tssreq2, buildManifestBuffer, devVals, basebandMode)){
                warning("[TSSR] failed to build tssrequest for alternative installType\n");
            }else{
                apticket2 = tss_request_send(tssreq2, server_url_string);
                if (print_tss_response) debug_plist(apticket2);
            }
            if (tssreq2) plist_free(tssreq2);
            devVals->installType = kInstallTypeDefault;
        }
        {
            plist_t tssreq2 = NULL;
            char *apnonce = devVals->apnonce;
            size_t apnonceLen = devVals->parsedApnonceLen;
            t_installType installType = devVals->installType;
            devVals->parsedApnonceLen = 0;
            devVals->apnonce = (char *)0x1337;
            devVals->installType = kInstallTypeErase;
            if (!tssrequest(&tssreq2, buildManifestBuffer, devVals, kBasebandModeWithoutBaseband)){
                apticket3 = tss_request_send(tssreq2, server_url_string);
                if (print_tss_response) debug_plist(apticket3);
            }
            devVals->parsedApnonceLen = apnonceLen;
            devVals->apnonce = apnonce;
            devVals->installType = installType;
        }
            
        plist_t manifest = 0;
        plist_from_xml(buildManifestBuffer, (unsigned)strlen(buildManifestBuffer), &manifest);
        plist_t build = plist_dict_get_item(manifest, "ProductBuildVersion");
        char *cbuild = 0;
        plist_get_string_val(build, &cbuild);
        plist_t pvers = plist_dict_get_item(manifest, "ProductVersion");
        char *cpvers = 0;
        plist_get_string_val(pvers, &cpvers);
        
        plist_t pecid = plist_dict_get_item(tssreq, "ApECID");
        plist_get_uint_val(pecid, &devVals->ecid);
        char *cecid = ecid_to_string(devVals->ecid);
        
        uint32_t size = 0;
        char* data = NULL;
        if (*devVals->generator)
            plist_dict_set_item(apticket, "generator", plist_new_string(devVals->generator));
        if (apticket2)
            plist_dict_set_item(apticket, "updateInstall", apticket2);
        if (apticket3)
            plist_dict_set_item(apticket, "noNonce", apticket3);
        plist_to_xml(apticket, &data, &size);
        
        char *apnonce = "";
        size_t apnonceLen = 0;
        
        if ((apnonceLen = devVals->parsedApnonceLen*2)){
            apnonce = (char*)malloc(apnonceLen+1);
            memset(apnonce, 0, apnonceLen+1);
            for (int i = 0; i < apnonceLen; i+=2)
                snprintf(&apnonce[i], apnonceLen-i+1, "%02x",((unsigned char*)devVals->apnonce)[i/2]);
        }
        
        size_t tmpDeviceNameSize = strlen(devVals->deviceModel) + strlen("_") + 1;
        if (devVals->deviceBoard) tmpDeviceNameSize += strlen(devVals->deviceBoard);
        
        char *tmpDevicename = (char *)malloc(tmpDeviceNameSize);
        memset(tmpDevicename, 0, tmpDeviceNameSize);
        snprintf(tmpDevicename, tmpDeviceNameSize, "%s", devVals->deviceModel);
        if (devVals->deviceBoard) snprintf(tmpDevicename+strlen(tmpDevicename), tmpDeviceNameSize-strlen(tmpDevicename), "_%s",devVals->deviceBoard);
        
        size_t fnamelen = strlen(shshSavePath) + 1 + strlen(cecid) + tmpDeviceNameSize + strlen(cpvers) + strlen(cbuild) + strlen(DIRECTORY_DELIMITER_STR"___-.shsh2") + 1;
        fnamelen += devVals->parsedApnonceLen*2;
        
        char *fname = malloc(fnamelen);
        memset(fname, 0, fnamelen);
        size_t prePathLen= strlen(shshSavePath);
        if (shshSavePath[prePathLen-1] == DIRECTORY_DELIMITER_CHR) prePathLen--;
        strncpy(fname, shshSavePath, prePathLen);
        
        snprintf(fname+prePathLen, fnamelen, DIRECTORY_DELIMITER_STR"%s_%s_%s-%s_%s.shsh%s",cecid,tmpDevicename,cpvers,cbuild, apnonce, (*devVals->generator || apticket2) ? "2" : "");
        
        
        FILE *shshfile = fopen(fname, "wb");
        if (!shshfile) error("[Error] can't save shsh at %s\n",fname);
        else{
            fwrite(data, strlen(data), 1, shshfile);
            fclose(shshfile);
            info("Saved shsh blobs!\n");
        }
        
        if (apnonceLen) free(apnonce);
        free(tmpDevicename);
        plist_free(manifest);
        free(fname);
        free(cpvers);
        free(cbuild);
        free(data);
    }
    
error:
    if (tssreq) plist_free(tssreq);
    if (apticket) plist_free(apticket);
    return isSigned;
#undef reterror
}

int isManifestSignedForDevice(const char *buildManifestPath, t_devicevals *devVals, t_iosVersion *versVals, const char* server_url_string){
    int isSigned = 0;
#define reterror(a ...) {error(a); isSigned = -1; goto error;}
    plist_t manifest = NULL;
    plist_t ProductVersion = NULL;
    plist_t SupportedProductTypes = NULL;
    plist_t mDevice = NULL;
    char *bufManifest = NULL;
    
    info("[TSSC] opening %s\n",buildManifestPath);
    //filehandling
    FILE *fmanifest = fopen(buildManifestPath, "r");
    if (!fmanifest) reterror("[TSSC] ERROR: file %s not found!\n",buildManifestPath);
    fseek(fmanifest, 0, SEEK_END);
    long fsize = ftell(fmanifest);
    fseek(fmanifest, 0, SEEK_SET);
    bufManifest = (char*)malloc(fsize + 1);
    bufManifest[fsize] = '\0';
    fread(bufManifest, fsize, 1, fmanifest);
    fclose(fmanifest);
    
    plist_from_xml(bufManifest, (unsigned)strlen(bufManifest), &manifest);
    if (!manifest)
        reterror("[TSSC] failed to load manifest\n");
    
    if (!versVals->version){
        ProductVersion = plist_dict_get_item(manifest, "ProductVersion");
        plist_get_string_val(ProductVersion, (char**)&versVals->version);
    }
    if (!devVals->deviceModel)
        reterror("[TSSC] can't proceed without device info\n");
    
    SupportedProductTypes = plist_dict_get_item(manifest, "SupportedProductTypes");
    if (SupportedProductTypes) {
        for (int i=0; i<plist_array_get_size(SupportedProductTypes); i++) {
            mDevice = plist_array_get_item(SupportedProductTypes, i);
            char *ldevice = NULL;
            plist_get_string_val(mDevice, &ldevice);
            if (strcasecmp(ldevice, devVals->deviceModel) == 0)
                goto checkedDeviceModel;
        }
    }
    
    reterror("[TSSC] selected device can't be used with that buildmanifest\n");
    
checkedDeviceModel:
    isSigned = isManifestBufSignedForDevice(bufManifest, devVals, versVals->basebandMode, server_url_string);
    
error:
    if (manifest) plist_free(manifest);
    if (bufManifest) free(bufManifest);
    return isSigned;
#undef reterror
}

int isVersionSignedForDevice(jssytok_t *firmwareTokens, t_iosVersion *versVals, t_devicevals *devVals, const char* server_url_string){
#define reterror(a ... ) {error(a); goto error;}
    int nocacheorig = nocache;
    if (versVals->version && atoi(versVals->version) <= 3) {
        info("[TSSC] version to check \"%s\" seems to be iOS 3 or lower, which did not require SHSH or APTicket.\n\tSkipping checks and returning true.\n",versVals->version);
        return 1;
    }
    
    int isSigned = -1;
    int isSignedOne = 0;
    char *buildManifest = NULL;
    
    t_versionURL *urls = getFirmwareUrls(devVals->deviceModel, versVals, firmwareTokens);
    if (!urls) reterror("[TSSC] ERROR: could not get url for device %s on iOS %s\n",devVals->deviceModel,(!versVals->version ? versVals->buildID : versVals->version));

    int cursigned = 0;
    for (t_versionURL *u = urls; u->url; u++) {
        buildManifest = getBuildManifest(u->url, devVals->deviceModel, versVals->version, u->buildID, versVals->isOta);
        if (!buildManifest) {
            error("[TSSC] ERROR: could not get BuildManifest for firmwareurl %s\n",u->url);
            continue;
        }

        if (cursigned && !u->isDupulicate) cursigned = 0;
        
        nocache = 1;
        
        if (cursigned) {
            info("[TSSC] skipping duplicated build\n");
            
        }else if ((isSignedOne = isManifestBufSignedForDevice(buildManifest, devVals, versVals->basebandMode, server_url_string)) >= 0){
            cursigned |= (isSigned > 0);
            
            isSigned = (isSignedOne > 0 || isSigned > 0);
            if (buildManifest) (void)(free(buildManifest)), buildManifest = NULL;
            info("Firmware version %s %s %s signed!\n",u->version,u->buildID,isSignedOne ? "IS" : "IS NOT");
        }
        (void)(free(u->url)),u->url = NULL;
        (void)(free(u->buildID)),u->buildID = NULL;
        (void)(free(u->version)),u->version = NULL;
    }
    (void)(free(urls)),urls = NULL;
    
error:
    nocache = nocacheorig;
    if (buildManifest) free(buildManifest);
    return isSigned;
#undef reterror
}

#pragma mark print functions
char *getFirmwareUrl(const char *deviceModel, t_iosVersion *versVals, jssytok_t *tokens){
    warning("FUNCTION IS DEPRECATED, USE getFirmwareUrls INSTEAD!\n");
    t_versionURL *versions, *v;
    versions = v = getFirmwareUrls(deviceModel, versVals, tokens);

    if (!versions)
        return NULL;
    char *ret = versions->url;
    free(versions->buildID);
    free(versions->version);
    
    while ((++versions)->url) {
        free(versions->buildID);
        free(versions->version);
        free(versions->url);
    }
    
    free(v);
    return ret;
}

/* Print devices function doesn't actually check if devices are sorted. it assues they are sorted in json */
int printListOfDevices(jssytok_t *tokens){
#define MAX_PER_LINE 10
    log("[JSON] printing device list\n");
    char *curr = NULL;
    size_t currLen = 0;
    int rspn = 0;
    putchar('\n');
    jssytok_t *ctok = jssy_dictGetValueForKey(tokens, "devices");
    
    jssytok_t *tmp = ctok->subval;
    for (size_t i=0; i<ctok->size; tmp = tmp->next,i++) {
        if (!curr){
            curr = tmp->value;
            currLen = tmp->size;
        }else{
            for (int i = 0; i<currLen; i++) {
                char c = curr[i];
                if (!(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))) break;
                if (c != tmp->value[i]) {
                    putchar('\n');
                    putchar('\n');
                    curr = tmp->value;
                    currLen = tmp->size;
                    rspn = 0;
                    break;
                }
            }
        }
        printJString(tmp);
        if (++rspn>= MAX_PER_LINE) (void)(putchar('\n')), rspn = 0; else putchar(' ');
        
    }
    putchar('\n');
    putchar('\n');
    return 0;
#undef MAX_PER_LINE
}

int cmpfunc(const void * a, const void * b){
    char *aa = *(char**)a;
    char *bb = *(char**)b;
    int d;
    
    while(1) {
        d = atoi(bb) - atoi(aa);
        if(d != 0)
            return d;
        aa = strchr(aa, '.');
        bb = strchr(bb, '.');
        if(aa == NULL || bb == NULL) {
            if(aa == NULL && bb == NULL) {
                aa = strchr(*(char**)a, '[');
                bb = strchr(*(char**)b, '[');
                return aa == NULL ? (bb == NULL ? 0 : -1) : 1;
            }
            return aa == NULL ? 1 : -1;
        }
        aa++;
        bb++;
    }
}

char **getListOfiOSForDevice(jssytok_t *tokens, const char *device, int isOTA, int *versionCntt){
    //requires free(versions[versionsCnt-1]); and free(versions); after use
    jssytok_t *firmwares = getFirmwaresForDevice(device, tokens, isOTA);
    
    if (!firmwares)
        return error("[TSSC] device %s could not be found in devicelist\n",device),NULL;
    
    int versionsCnt = (int)firmwares->size;
    char **versions = (char**)malloc(versionsCnt * sizeof(char *));
    
    jssytok_t *tmp = firmwares->subval;
    for (int i=0; i<firmwares->size; tmp = tmp->next,i++) {
        
        jssytok_t *ios = jssy_dictGetValueForKey(tmp, "version");
        
        int isBeta = 0;
        jssytok_t *releaseType = NULL;
        if ((releaseType = jssy_dictGetValueForKey(tmp, "releasetype"))) {
            isBeta = (strncmp(releaseType->value, "Beta", releaseType->size) == 0);
        }
        
        versions[--versionsCnt] = (char*)malloc((ios->size+1 + isBeta * strlen("[B]")) * sizeof(char));
        strncpy(versions[versionsCnt], ios->value, ios->size);
        if (isBeta) strncpy(&versions[versionsCnt][ios->size], "[B]", strlen("[B]"));
        versions[versionsCnt][ios->size + isBeta * strlen("[B]")] = '\0';
    }
    versionsCnt = (int)firmwares->size;
    qsort(versions, versionsCnt, sizeof(char *), &cmpfunc);
    if (versionCntt) *versionCntt = versionsCnt;
    return versions;
}

int printListOfiOSForDevice(jssytok_t *tokens, char *device, int isOTA){
#define MAX_PER_LINE 10
    
    int versionsCnt;
    char **versions = getListOfiOSForDevice(tokens, device, isOTA, &versionsCnt);
    
    int rspn = 0,
        currVer = 0,
        nextVer = 0;
    for (int i=0; i<versionsCnt; i++) {
        if (i){
            int res = strcmp(versions[i-1], versions[i]);
            free(versions[i-1]);
            if (res == 0) continue;
        }
        
        nextVer = atoi(versions[i]);
        if (currVer && currVer != nextVer) (void)(printf("\n")), rspn = 0;
        currVer = nextVer;
        if (!rspn) printf("[iOS %2i] ",currVer);
        int printed = 0;
        printf("%s%n",versions[i],&printed);
        while (printed++ < 12) putchar(' ');
        if (++rspn>= MAX_PER_LINE) (void)(putchar('\n')), rspn = 0; else putchar(' ');
    }
    free(versions[versionsCnt-1]);
    free(versions);
    
    printf("\n\n");
    return 0;
#undef MAX_PER_LINE
}

#pragma mark check functions
jssytok_t *getFirmwaresForDevice(const char *device, jssytok_t *tokens, int isOta){
    jssytok_t *ctok = (isOta) ? tokens : jssy_dictGetValueForKey(tokens, "devices");
    
    jssytok_t *tmp = ctok->subval;
    for (int i=0; i<ctok->size; tmp = tmp->next,i++)
        if (strncasecmp(device, tmp->value, tmp->size) == 0
            && strlen(device) == tmp->size)
            return jssy_dictGetValueForKey(tmp->subval, "firmwares");
    
    return NULL;
}

int checkFirmwareForDeviceExists(t_devicevals *devVals, t_iosVersion *versVals, jssytok_t *tokens){
    jssytok_t *firmwares = getFirmwaresForDevice(devVals->deviceModel, tokens, versVals->isOta);
    if (!firmwares)
        return 0;
    
    const char *versstr = (versVals->buildID) ? versVals->buildID : versVals->version;
    jssytok_t *tt = firmwares->subval;
    for (int i=0; i<firmwares->size;tt = tt->next,i++) {
        
        jssytok_t *versiont = jssy_dictGetValueForKey(tt, (versVals->buildID) ? "buildid" : "version");
        if (versiont->size == strlen(versstr) && strncmp(versstr, versiont->value, versiont->size) == 0)
            return 1;
    }
    
    return 0;
}

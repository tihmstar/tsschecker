//
//  ipswme.c
//  tsschecker
//
//  Created by tihmstar on 07.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "tsschecker.h"
#include "jsmn.h"
#include "download.h"
#include <libfragmentzip/libfragmentzip.h>
#include <libirecovery.h>
#include "tss.h"

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA1(d, n, md) CC_SHA1(d, n, md)
#   define SHA384(d, n, md) CC_SHA384(d, n, md)
#else
#   include <openssl/sha.h>
#endif // __APPLE__

#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json/condensed"
#define FIRMWARE_OTA_JSON_URL "https://api.ipsw.me/v2.1/ota.json/condensed"

#warning TODO verify these values are actually correct for all devices (iPhone7)
#define NONCELEN_BASEBAND 20 
#define NONCELEN_SEP      20

#define swapchar(a,b) ((a) ^= (b),(b) ^= (a),(a) ^= (b)) //swaps a and b, unless they are the same variable


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
    "\\firmware.json"
};

static const char *win_path_get(enum paths path){
    if (!win_path_didinit) memset(win_paths, 0, sizeof(win_paths));
    if (win_paths[path]) return win_paths[path];
    
    const char *tmp = getenv("TMP");
    if (tmp && *tmp){
        size_t len = strlen(tmp) + strlen(win_pathvars[path]) + 1;
        win_paths[path] = (char *)malloc(len);
        memset((char*)win_paths[path], '\0', len);
        strncat((char*)win_paths[path], tmp, strlen(tmp));
        strncat((char*)win_paths[path], win_pathvars[path], strlen(win_pathvars[path]));
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
#define FIRMWARE_JSON_PATH "/tmp/firmware.json"
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


static struct bbdevice bbdevices[] = {
    {"iPod1,1", 0},
    {"iPod2,1", 0},
    {"iPod3,1", 0},
    {"iPod4,1", 0},
    {"iPod5,1", 0},
    {"iPod7,1", 0},
    
    {"iPhone2,1", 0},
    {"iPhone3,1", 257},
    {"iPhone3,3", 2},
    {"iPhone4,1", 2},
    {"iPhone5,1", 3255536192},
    {"iPhone5,2", 3255536192},
    {"iPhone5,3", 3554301762},
    {"iPhone5,4", 3554301762},
    {"iPhone6,1", 3554301762},
    {"iPhone6,2", 3554301762},
    {"iPhone7,1", 3840149528},
    {"iPhone7,2", 3840149528},
    {"iPhone8,1", 3840149528},
    {"iPhone8,2", 3840149528},
    {"iPhone8,4", 3840149528},
    
    {"iPhone9,3", 1421084145},
    {"iPhone9,4", 1421084145},
    
    {"iPad1,1", 0},
    {"iPad2,1", 0},
    {"iPad2,2", 257},
    {"iPad2,4", 0},
    {"iPad2,5", 0},
    {"iPad3,1", 0},
    {"iPad3,2", 4},
    {"iPad3,3", 4},
    {"iPad3,4", 0},
    {"iPad3,6", 3255536192},
    {"iPad4,1", 0},
    {"iPad4,2", 3554301762},
    {"iPad4,4", 0},
    {"iPad4,5", 3554301762},
    {"iPad4,8", 3554301762},
    {"iPad5,2", 3840149528},
    {"iPad5,3", 0},
    {"iPad5,4", 3840149528},
    {"iPad6,3", 0},
    {"iPad6,4", 3840149528},
    {"iPad6,7", 0},
    {"iPad6,8", 3840149528},
    
    {"AppleTV1,1", 0},
    {"AppleTV2,1", 0},
    {"AppleTV3,1", 0},
    {"AppleTV3,2", 0},
    {"AppleTV5,3", 0},
    {NULL,0}
};

t_bbdevice bbdevices_get_all() {
    return bbdevices;
}

char *getFirmwareJson(){
    info("[TSSC] opening firmware.json\n");
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
    plist_t rt = NULL;
#define reterror(a ... ) {error(a); rt = NULL; goto error;}
    
    plist_t buildidentities = plist_dict_get_item(buildManifest, "BuildIdentities");
    if (!buildidentities || plist_get_node_type(buildidentities) != PLIST_ARRAY){
        reterror("[TSSR] Error: could not get BuildIdentities\n");
    }
    for (int i=0; i<plist_array_get_size(buildidentities); i++) {
        rt = plist_array_get_item(buildidentities, i);
        if (!rt || plist_get_node_type(rt) != PLIST_DICT){
            reterror("[TSSR] Error: could not get id%d\n",i);
        }
        plist_t infodict = plist_dict_get_item(rt, "Info");
        if (!infodict || plist_get_node_type(infodict) != PLIST_DICT){
            reterror("[TSSR] Error: could not get infodict\n");
        }
        plist_t RestoreBehavior = plist_dict_get_item(infodict, "RestoreBehavior");
        if (!RestoreBehavior || plist_get_node_type(RestoreBehavior) != PLIST_STRING){
            reterror("[TSSR] Error: could not get RestoreBehavior\n");
        }
        char *string = NULL;
        plist_get_string_val(RestoreBehavior, &string);
        //assuming there are only Erase and Update. If it's not Erase it must be Update
        //also converting isUpdateInstall to bool (1 or 0)
        if ((strncmp(string, "Erase", strlen(string)) != 0) == !isUpdateInstall){
            //continue when Erase found but isUpdateInstall is true
            //or Update found and isUpdateInstall is false
            rt = NULL;
            continue;
        }
        
        plist_t DeviceClass = plist_dict_get_item(infodict, "DeviceClass");
        if (!DeviceClass || plist_get_node_type(DeviceClass) != PLIST_STRING){
            reterror("[TSSR] Error: could not get DeviceClass\n");
        }
        plist_get_string_val(DeviceClass, &string);
        if (strcasecmp(string, boardconfig) != 0)
            rt = NULL;
        else
            break;
        
    }
    
error:
    return rt;
#undef reterror
}

plist_t getBuildidentity(plist_t buildManifest, const char *model, int isUpdateInstall){
    plist_t rt = NULL;
#define reterror(a ... ) {error(a); rt = NULL; goto error;}
    
    const char *boardconfig = getBoardconfigFromModel(model);
    if (!boardconfig)
        reterror("[TSSR] cant find boardconfig for device=%s please manuall use --boardconfig\n",model);
    
    rt = getBuildidentityWithBoardconfig(buildManifest, boardconfig, isUpdateInstall);
    
error:
    return rt;
#undef reterror
}


#pragma mark json functions

void printJString(jsmntok_t *str, char * firmwarejson){
    for (int j=0; j<str->end-str->start; j++) {
        putchar(*(firmwarejson+str->start + j));
    }
    putchar('\n');
}

jsmntok_t *objectForKey(jsmntok_t *tokens, const char *firmwareJson, const char*key){
    
    jsmntok_t *dictElements = tokens->value;
    for (jsmntok_t *tmp = dictElements; ; tmp = tmp->next) {
        
        if (strncmp(key, firmwareJson + tmp->start, strlen(key)) == 0) return tmp;
        
        if (tmp->next == dictElements) break;
    }
    
    return NULL;
}

int parseTokens(const char *json, jsmntok_t **tokens){
    jsmn_parser parser;
    jsmn_init(&parser);
    
    log("[JSON] counting elements\n");
    unsigned int tokensCnt = jsmn_parse(&parser, json, strlen(json), NULL, 0);
    
    *tokens = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokensCnt);
    jsmn_init(&parser);
    log("[JSON] parsing elements\n");
    return jsmn_parse(&parser, json, strlen(json), *tokens, tokensCnt);
}

#pragma mark get functions

//returns NULL terminated array of t_versionURL objects
t_versionURL *getFirmwareUrls(const char *deviceModel, t_iosVersion *versVals, const char *firmwarejson, jsmntok_t *tokens){
#define jstrSize(j) (j->end - j->start)
    t_versionURL *rets = NULL;
    const t_versionURL *rets_base = NULL;
    unsigned retcounter = 0;
    
    jsmntok_t *firmwares = getFirmwaresForDevice(deviceModel, firmwarejson, tokens, versVals->isOta);
    const char *versstring = (versVals->buildID) ? versVals->buildID : versVals->version;
    
    if (!firmwares)
        return error("[TSSC] device '%s' could not be found in devicelist\n", deviceModel), NULL;
    
malloc_rets:
    if (retcounter)
        memset(rets = (t_versionURL*)malloc(sizeof(t_versionURL)*(retcounter+1)), 0, sizeof(t_versionURL)*(retcounter+1));
    rets_base = rets;
    
    for (jsmntok_t *tmp = firmwares->value; tmp != NULL; tmp = tmp->next) {
        jsmntok_t *ios = objectForKey(tmp, firmwarejson, (versVals->buildID) ? "buildid" : "version");
        if (ios->value->end - ios->value->start == strlen(versstring) && strncmp(versstring, firmwarejson + ios->value->start, strlen(versstring)) == 0) {
            
            if (versVals->isOta) {
                jsmntok_t *releaseType = NULL;
                if (versVals->useBeta && !(releaseType = objectForKey(tmp, firmwarejson, "releasetype"))) continue;
                else if (!versVals->useBeta);
                else if (strncmp(firmwarejson + releaseType->value->start, "Beta", releaseType->value->end - releaseType->value->start) != 0) continue;
            }
            
            jsmntok_t *url = objectForKey(tmp, firmwarejson, "url")->value;
            jsmntok_t *i_vers = objectForKey(tmp, firmwarejson, "version")->value;
            jsmntok_t *i_build = objectForKey(tmp, firmwarejson, "buildid")->value;
            
            if (!versVals->version){
                versVals->version = (char*)malloc(i_vers->size+1);
                memcpy(versVals->version, firmwarejson+i_vers->start, i_vers->size);
                versVals->version[i_vers->size] = '\0';
            }
            
            
            if (!rets) retcounter++;
            else{
                int skip = 0;
                for (int i=0; rets_base[i].buildID; i++) {
                    if (strncmp(rets_base[i].buildID, firmwarejson + i_build->start, jstrSize(i_build)) == 0){
                        info("[TSSC] skipping duplicated buildid %s\n",rets_base[i].buildID);
                        skip = 1;
                        break;
                    }
                }
                if (skip) continue;
                
                info("[TSSC] got firmwareurl for iOS %.*s build %.*s\n",i_vers->end - i_vers->start,firmwarejson + i_vers->start,i_build->end - i_build->start,firmwarejson + i_build->start);
                rets->version = (char*)malloc(jstrSize(i_vers)+1);
                memcpy(rets->version, firmwarejson+i_vers->start, jstrSize(i_vers));
                rets->version[jstrSize(i_vers)] = '\0';
                
                rets->buildID = (char*)malloc(jstrSize(i_build)+1);
                memcpy(rets->buildID, firmwarejson+i_build->start, jstrSize(i_build));
                rets->buildID[jstrSize(i_build)] = '\0';
                
                rets->url = (char*)malloc(jstrSize(url)+1);
                memcpy(rets->url, firmwarejson+url->start, jstrSize(url));
                rets->url[jstrSize(url)] = '\0';
                rets++;
            }
        }
    }
    if (!retcounter) return NULL;
    else if (!rets) goto malloc_rets;
    
    
    return (t_versionURL*)rets_base;
#undef jstrSize
}

char *getFirmwareUrl(const char *deviceModel, t_iosVersion *versVals, const char *firmwarejson, jsmntok_t *tokens){
    warning("FUNCTION IS DEPRECATED, USE getFirmwareUrls INSTEAD!\n");
    t_versionURL *versions, *v;
    versions = v = getFirmwareUrls(deviceModel, versVals, firmwarejson, tokens);
    
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

static void printline(int percent){
    info("%03d [",percent);for (int i=0; i<100; i++) putchar((percent >0) ? ((--percent > 0) ? '=' : '>') : ' ');
    info("]");
}

static void fragmentzip_callback(unsigned int progress){
    info("\x1b[A\033[J"); //clear 2 lines
    printline((int)progress);
    info("\n");
}

int downloadPartialzip(const char *url, const char *file, const char *dst){
    log("[LFZP] downloading %s from %s\n\n",file,url);
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
    
    strncat(fileDir, MANIFEST_SAVE_PATH, strlen(MANIFEST_SAVE_PATH));
    strncat(fileDir, DIRECTORY_DELIMITER_STR, 1);
    strncat(fileDir, device, strlen(device));
    strncat(fileDir, "_", strlen("_"));
    strncat(fileDir, version, strlen(version));
    if (buildID){
        strncat(fileDir, "_", strlen("_"));
        strncat(fileDir, buildID, strlen(buildID));
    }
    
    if (isOta) strncat(fileDir, "ota", strlen("ota"));
    
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
        if (downloadPartialzip(url, (isOta) ? "AssetData/boot/BuildManifest.plist" : "BuildManifest.plist", fileDir)){
            free(fileDir);
            return NULL;
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

int64_t getBBGCIDForDevice(const char *deviceModel){
    
    t_bbdevice bbdevs = bbdevices_get_all();
    
    while (bbdevs->deviceModel && strcasecmp(bbdevs->deviceModel, deviceModel) != 0)
        bbdevs++;
    
    if (!bbdevs->deviceModel) {
        error("[TSSC] ERROR: device \"%s\" is not in bbgcid list, which means it's BasebandGoldCertID isn't documented yet.\nIf you own such a device please consider contacting @tihmstar to get instructions how to contribute to this project.\n",deviceModel);
        return -1;
    }else if (!bbdevs->bbgcid) {
        warning("[TSSC] A BasebandGoldCertID is not required for %s\n",deviceModel);
    }
    
    return bbdevs->bbgcid;
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

int tss_populate_basebandvals(plist_t tssreq, plist_t tssparameters, int64_t BbGoldCertId, size_t bbsnumSize){
    int ret = 0;
    plist_t parameters = plist_new_dict();
    char bbnonce[NONCELEN_BASEBAND+1];
    char *bbsnum = (char*)malloc(bbsnumSize);
    int64_t BbChipID = 0;
    
    
    getRandNum(bbsnum, bbsnumSize, 256);
    getRandNum(bbnonce, NONCELEN_BASEBAND, 256);
    srand((unsigned int)time(NULL));
    int n=0; for (int i=1; i<7; i++) BbChipID += (rand() % 10) * pow(10, ++n);
    
    
    /* BasebandNonce not required */
//    plist_dict_set_item(parameters, "BbNonce", plist_new_data(bbnonce, noncelen));
    plist_dict_set_item(parameters, "BbChipID", plist_new_uint(BbChipID));
    plist_dict_set_item(parameters, "BbGoldCertId", plist_new_uint(BbGoldCertId));
    plist_dict_set_item(parameters, "BbSNUM", plist_new_data(bbsnum, bbsnumSize));
    
    /* BasebandFirmware */
    plist_t BasebandFirmware = plist_access_path(tssparameters, 2, "Manifest", "BasebandFirmware");
    if (!BasebandFirmware || plist_get_node_type(BasebandFirmware) != PLIST_DICT) {
        error("ERROR: Unable to get BasebandFirmware node\n");
        ret = -1;
        goto error;
    }
    plist_t bbfwdict = plist_copy(BasebandFirmware);
    BasebandFirmware = NULL;
    if (plist_dict_get_item(bbfwdict, "Info")) {
        plist_dict_remove_item(bbfwdict, "Info");
    }
    plist_dict_set_item(tssreq, "BasebandFirmware", bbfwdict);
    
    tss_request_add_baseband_tags(tssreq, parameters, NULL);
    
error:
    free(bbsnum);
    return ret;
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
        if ((next =! next) && nlen < nonceLen) ret[nlen++] = tmp,tmp=0;
    }
    
    if (parsedLen) *parsedLen = nlen;
    return 0;
}

int tss_populate_random(plist_t tssreq, int is64bit, t_devicevals *devVals){
    size_t nonceLen = 20; //valid for all devices up to iPhone7
    if (!devVals->deviceModel)
        return error("[TSSR] internal error: devVals->deviceModel is missing\n"),-1;
    
    if (strncasecmp(devVals->deviceModel, "iPhone9,", strlen("iPhone9,")) == 0)
        nonceLen = 32;
    
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
        if (nonceLen == 20) {
            //this is a pre iPhone7 device
            //nonce is derived from generator with SHA1
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
            unsigned char zz[8] = {0};
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
    debug("[TSSR] nonce=%s\n",devVals->apnonce);
    debug("[TSSR] sepnonce=%s\n",devVals->sepnonce);
    
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
        warning("[TSSC] could not get id0 for installType=Erase. Using fallback installType=Update since user did not specify installType manually\n");

        devVals->installType = kInstallTypeUpdate;
        goto getID0;
    }
    
    if (!id0 || plist_get_node_type(id0) != PLIST_DICT){
        reterror("[TSSR] Error: could not get id0 for installType=%s\n",devVals->isUpdateInstall ? "Update" : "Erase");
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
        //TODO don't use .shsh2 ending and don't save generator when saving only baseband
        info("[TSSR] User specified to request only a Baseband ticket.\n");
    }
    
    if (basebandMode != kBasebandModeWithoutBaseband) {
        //TODO: verify that this being int64_t instead of uint64_t doesn't actually break something
        
        int64_t BbGoldCertId = (devVals->bbgcid) ? devVals->bbgcid : getBBGCIDForDevice(devVals->deviceModel);
        if (BbGoldCertId == -1) {
            if (basebandMode == kBasebandModeOnlyBaseband){
                reterror("[TSSR] failed to get BasebandGoldCertID, but requested to get only baseband ticket. Aborting here!\n");
            }
            warning("[TSSR] there was an error getting BasebandGoldCertID, continuing without requesting Baseband ticket\n");
        }else if (BbGoldCertId) {
            
            size_t bbsnumSize = 4;
            if (strncasecmp(devVals->deviceModel, "iPhone9,", strlen("iPhone9,")) == 0)
                bbsnumSize = 12;
            
            tss_populate_basebandvals(tssreq,tssparameter,BbGoldCertId,bbsnumSize);
            tss_request_add_baseband_tags(tssreq, tssparameter, NULL);
        }else{
            log("[TSSR] LOG: device %s doesn't need a Baseband ticket, continuing without requesting a Baseband ticket\n",devVals->deviceModel);
        }
    }else{
        info("[TSSR] User specified not to request a Baseband ticket.\n");
    }
    
    *tssreqret = tssreq;
error:
    if (manifest) plist_free(manifest);
    if (tssparameter) plist_free(tssparameter);
    if (error) plist_free(tssreq), *tssreqret = NULL;
    return error;
#undef reterror
}

int isManifestBufSignedForDevice(char *buildManifestBuffer, t_devicevals *devVals, t_basebandMode basebandMode){
    int isSigned = 0;
    plist_t tssreq = NULL;
    plist_t apticket = NULL;
    plist_t apticket2 = NULL;
    plist_t apticket3 = NULL;
    
    if (tssrequest(&tssreq, buildManifestBuffer, devVals, basebandMode)){
        error("[TSSR] faild to build tssrequest\n");
        goto error;
    }
    isSigned = ((apticket = tss_request_send(tssreq, NULL)) > 0);

    
    if (print_tss_response) debug_plist(apticket);
    if (isSigned && save_shshblobs){
        if (!devVals->installType){
            plist_t tssreq2 = NULL;
            info("also requesting APTicket for installType=Update\n");
            devVals->installType = kInstallTypeUpdate;
            if (tssrequest(&tssreq2, buildManifestBuffer, devVals, basebandMode)){
                warning("[TSSR] faild to build tssrequest for alternative installType\n");
            }else{
                apticket2 = tss_request_send(tssreq2, NULL);
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
            if (!tssrequest(&tssreq2, buildManifestBuffer, devVals, basebandMode)){
                apticket3 = tss_request_send(tssreq2, NULL);
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
        
        
        FILE *shshfile = fopen(fname, "w");
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
}

int isManifestSignedForDevice(const char *buildManifestPath, t_devicevals *devVals, t_iosVersion *versVals){
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
    if (!fmanifest) reterror("[TSSC] ERROR: file %s nof found!\n",buildManifestPath);
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
    isSigned = isManifestBufSignedForDevice(bufManifest, devVals, versVals->basebandMode);
    
error:
    if (manifest) plist_free(manifest);
    if (bufManifest) free(bufManifest);
    return isSigned;
#undef reterror
}

int isVersionSignedForDevice(char *firmwareJson, jsmntok_t *firmwareTokens, t_iosVersion *versVals, t_devicevals *devVals){
#define reterror(a ... ) {error(a); goto error;}
    if (atoi(versVals->version) <= 3) {
        info("[TSSC] version to check \"%s\" seems to be iOS 3 or lower, which did not require SHSH or APTicket.\n\tSkipping checks and returning true.\n",versVals->version);
        return 1;
    }
    
    int isSigned = 0;
    int isSignedOne = 0;
    char *buildManifest = NULL;
    
    t_versionURL *urls = getFirmwareUrls(devVals->deviceModel, versVals, firmwareJson, firmwareTokens);
    if (!urls) reterror("[TSSC] ERROR: could not get url for device %s on iOS %s\n",devVals->deviceModel,versVals->version);

    for (t_versionURL *u = urls; u->url; u++) {
        buildManifest = getBuildManifest(u->url, devVals->deviceModel, versVals->version, u->buildID, versVals->isOta);
        if (!buildManifest) {
            error("[TSSC] ERROR: could not get BuildManifest for firmwareurl %s\n",u->url);
            continue;
        }
        
        isSigned |= isSignedOne  = isManifestBufSignedForDevice(buildManifest, devVals, versVals->basebandMode);
        if (buildManifest) free(buildManifest), buildManifest = NULL;
        info("iOS %s %s %s signed!\n",u->version,u->buildID,isSignedOne ? "IS" : "IS NOT");
        free(u->url),u->url = NULL;
        free(u->buildID),u->buildID = NULL;
        free(u->version),u->version = NULL;
    }
    free(urls),urls = NULL;
    
    
    
error:
    if (buildManifest) free(buildManifest);
    return isSigned;
#undef reterror
}

#pragma mark print functions

#warning print devices function doesn't actually check if devices are sorted. it assues they are sorted in json
int printListOfDevices(char *firmwarejson, jsmntok_t *tokens){
#define MAX_PER_LINE 10
    log("[JSON] printing device list\n");
    int curr = 0;
    int currLen = 0;
    int rspn = 0;
    putchar('\n');
    jsmntok_t *ctok = objectForKey(tokens, firmwarejson, "devices");
    for (jsmntok_t *tmp = ctok->value; ; tmp = tmp->next) {
        if (!curr){
            curr = tmp->start;
            currLen = tmp->end - tmp->start;
        }else{
            for (int i = 0; i<currLen; i++) {
                char c = *(firmwarejson+curr+i);
                if (!(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z'))) break;
                if (c != *(firmwarejson+tmp->start+i)) {
                    putchar('\n');
                    putchar('\n');
                    curr = tmp->start;
                    currLen = tmp->end - tmp->start;
                    rspn = 0;
                    break;
                }
            }
        }
        for (int j=0; j<tmp->end-tmp->start; j++) {
            putchar(*(firmwarejson+tmp->start + j));
        }
        if (++rspn>= MAX_PER_LINE) putchar('\n'), rspn = 0; else putchar(' ');
        
        if (tmp->next == ctok->value) break;
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

char **getListOfiOSForDevice(char *firmwarejson, jsmntok_t *tokens, const char *device, int isOTA, int *versionCntt){
    //requires free(versions[versionsCnt-1]); and free(versions); after use
    jsmntok_t *firmwares = getFirmwaresForDevice(device, firmwarejson, tokens, isOTA);
    
    if (!firmwares)
        return error("[TSSC] device %s could not be found in devicelist\n",device),NULL;
    
    int versionsCnt = firmwares->size;
    char **versions = (char**)malloc(versionsCnt * sizeof(char *));
    
    
    for (jsmntok_t *tmp = firmwares->value; tmp != NULL; tmp = tmp->next) {
        
        jsmntok_t *ios = objectForKey(tmp, firmwarejson, "version");
        
        int verslen= ios->value->end-ios->value->start;
        int isBeta = 0;
        jsmntok_t *releaseType = NULL;
        if ((releaseType = objectForKey(tmp, firmwarejson, "releasetype"))) {
            isBeta = (strncmp(firmwarejson + releaseType->value->start, "Beta", releaseType->value->end - releaseType->value->start) == 0);
        }
        
        versions[--versionsCnt] = (char*)malloc((verslen+1 + isBeta * strlen("[B]")) * sizeof(char));
        strncpy(versions[versionsCnt], firmwarejson + ios->value->start, verslen);
        if (isBeta) strncpy(&versions[versionsCnt][verslen], "[B]", strlen("[B]"));
        versions[versionsCnt][verslen + isBeta * strlen("[B]")] = '\0';
    }
    versionsCnt = firmwares->size;
    qsort(versions, versionsCnt, sizeof(char *), &cmpfunc);
    if (versionCntt) *versionCntt = versionsCnt;
    return versions;
}


int printListOfiOSForDevice(char *firmwarejson, jsmntok_t *tokens, char *device, int isOTA){
#define MAX_PER_LINE 10
    
    int versionsCnt;
    char **versions = getListOfiOSForDevice(firmwarejson, tokens, device, isOTA, &versionsCnt);
    
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
        if (currVer && currVer != nextVer) printf("\n"), rspn = 0;
        currVer = nextVer;
        if (!rspn) printf("[iOS %2i] ",currVer);
        int printed = 0;
        printf("%s%n",versions[i],&printed);
        while (printed++ < 12) putchar(' ');
        if (++rspn>= MAX_PER_LINE) putchar('\n'), rspn = 0; else putchar(' ');
    }
    free(versions[versionsCnt-1]);
    free(versions);
    
    printf("\n\n");
    return 0;
#undef MAX_PER_LINE
}


#pragma mark check functions

jsmntok_t *getFirmwaresForDevice(const char *device, const char *firmwareJson, jsmntok_t *tokens, int isOta){
    jsmntok_t *ctok = (isOta) ? tokens : objectForKey(tokens, firmwareJson, "devices");
    for (jsmntok_t *tmp = ctok->value; ; tmp = tmp->next) {
        
        if (strncasecmp(device, firmwareJson+tmp->start, tmp->end - tmp->start) == 0)
            return objectForKey(tmp, firmwareJson, "firmwares");
        
        if (tmp->next == ctok->value) break;
    }
    
    return NULL;
}

int checkFirmwareForDeviceExists(t_devicevals *devVals, t_iosVersion *versVals, char *firmwareJson, jsmntok_t *tokens){
    
    jsmntok_t *firmwares = getFirmwaresForDevice(devVals->deviceModel, firmwareJson, tokens, versVals->isOta);
    if (!firmwares)
        return 0;
    
    const char *versstr = (versVals->buildID) ? versVals->buildID : versVals->version;
    for (jsmntok_t *tt = firmwares->value; tt ;tt = tt->next) {
        
        jsmntok_t *versiont = objectForKey(tt, firmwareJson, (versVals->buildID) ? "buildid" : "version");
        size_t len = versiont->value->end - versiont->value->start;
        if (len == strlen(versstr) && strncmp(versstr, firmwareJson + versiont->value->start, len) == 0) return 1;
    }
    
    return 0;
}

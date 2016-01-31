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
#include "tsschecker.h"
#include "jsmn.h"
#include "download.h"
#include <libpartialzip-1.0/libpartialzip.h>
#include "tss.h"

#define FIRMWARE_JSON_PATH "/tmp/firmware.json"
//#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json"
#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json/condensed"
#define FIRMWARE_OTA_JSON_PATH "/tmp/ota.json"
#define FIRMWARE_OTA_JSON_URL "https://api.ipsw.me/v2.1/ota.json/condensed"

#define MANIFEST_SAVE_PATH "/tmp/tsschecker"
#define noncelen 20

char *getFirmwareJson(){
    info("[TSSC] opening firmware.json\n");
    FILE *f = fopen(FIRMWARE_JSON_PATH, "rb");
    
    if (!f){
        downloadFile(FIRMWARE_JSON_URL, FIRMWARE_JSON_PATH);
        f = fopen(FIRMWARE_JSON_PATH, "rb");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *firmwareJson = malloc(fsize + 1);
    fread(firmwareJson, fsize, 1, f);
    fclose(f);
    
    return firmwareJson;
}

char *getOtaJson(){
    info("[TSSC] opening ota.json\n");
    FILE *f = fopen(FIRMWARE_OTA_JSON_PATH, "rb");
    
    if (!f){
        downloadFile(FIRMWARE_OTA_JSON_URL, FIRMWARE_OTA_JSON_PATH);
        f = fopen(FIRMWARE_OTA_JSON_PATH, "rb");
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *firmwareJson = malloc(fsize + 1);
    fread(firmwareJson, fsize, 1, f);
    fclose(f);
    
    return firmwareJson;
}

#pragma mark json functions

void printJString(jsmntok_t *str, char * firmwarejson){
    for (int j=0; j<str->end-str->start; j++) {
        putchar(*(firmwarejson+str->start + j));
    }
    putchar('\n');
}

jsmntok_t *objectForKey(jsmntok_t *tokens, char *firmwareJson, char*key){
    
    jsmntok_t *dictElements = tokens->value;
    for (jsmntok_t *tmp = dictElements; ; tmp = tmp->next) {
        
        if (strncmp(key, firmwareJson + tmp->start, strlen(key)) == 0) return tmp;
        
        if (tmp->next == dictElements) break;
    }
    
    return NULL;
}

int parseTokens(char *json, jsmntok_t **tokens){
    jsmn_parser parser;
    jsmn_init(&parser);
    
    log("[JSON] counting emlements\n");
    unsigned int tokensCnt = jsmn_parse(&parser, json, strlen(json), NULL, 0);
    
    *tokens = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokensCnt);
    jsmn_init(&parser);
    log("[JSON] parsing emlements\n");
    return jsmn_parse(&parser, json, strlen(json), *tokens, tokensCnt);
}

#define get functions

char *getFirmwareUrl(char *device, char *version,char *firmwarejson, jsmntok_t *tokens, int isOta){
    
    jsmntok_t *devices = (isOta) ? tokens : objectForKey(tokens, firmwarejson, "devices");
    jsmntok_t *mydevice = objectForKey(devices, firmwarejson, device);
    jsmntok_t *firmwares = objectForKey(mydevice, firmwarejson, "firmwares");
    
    for (jsmntok_t *tmp = firmwares->value; tmp != NULL; tmp = tmp->next) {
        jsmntok_t *ios = objectForKey(tmp, firmwarejson, "version");
        if (strncmp(version, firmwarejson + ios->value->start, strlen(version)) == 0) {
            
            jsmntok_t *url = objectForKey(tmp, firmwarejson, "url")->value;
            
            char *ret = malloc(url->end - url->start+1);
            char *cpy = ret;
            memset(ret, 0, url->end - url->start+1);
            for (int i=url->start ; i< url->end; i++) *(cpy++) = firmwarejson[i];
            return ret;
        }
        
    }
    return NULL;
}

static void printline(int percent){
    info("%03d [",percent);for (int i=0; i<100; i++) putchar((percent >0) ? ((--percent > 0) ? '=' : '>') : ' ');
    info("]");
}

static void partialzip_callback(partialzip_t* info, partialzip_file_t* file, size_t progress){
    info("\x1b[A\033[J"); //clear 2 lines
    printline((int)progress);
    info("\n");
}

int downloadPartialzip(char *url, char *file, char *dst){
    log("[LPZP] downloading %s from %s\n",file,url);
    return partialzip_download_file(url, file, dst, &partialzip_callback);
}

char *getBuildManifest(char *url, int isOta){
    struct stat st = {0};
    
    //gen name
    size_t len = strlen(url)-1;
    char *name = NULL;
    while (url[--len] != '/') name = url+len;
    
    len = strlen(url+len) + strlen(MANIFEST_SAVE_PATH) + 1 +1;
    if (isOta) len += strlen("ota");
    char *fileDir = malloc(len);
    memset(fileDir, 0, len);
    
    strncat(fileDir, MANIFEST_SAVE_PATH, strlen(MANIFEST_SAVE_PATH));
    strncat(fileDir, "/", 1);
    strncat(fileDir, name, strlen(MANIFEST_SAVE_PATH));
    if (isOta) strncat(fileDir, "ota", strlen("ota"));

    memset(&st, 0, sizeof(st));
    if (stat(MANIFEST_SAVE_PATH, &st) == -1) mkdir(MANIFEST_SAVE_PATH, 0700);
    
    //get file
    info("[TSSC] opening Buildmanifest for %s\n",name);
    FILE *f = fopen(fileDir, "rb");
    
    if (!f){
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

void debug_plist(plist_t plist);

void getRandNum(char *dst, size_t size, int base){
    for (int i=0; i<size; i++) {
        int j;
        if (base == 256) dst[i] = arc4random() % base;
        else dst[i] = ((j = arc4random() % base) < 10) ? '0' + j : 'a' + j-10;
    }
}

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

int tss_populate_basebandvals(plist_t tssreq, plist_t tssparameters, int64_t BbGoldCertId){
    plist_t parameters = plist_new_dict();
    char bbnonce[noncelen+1];
    char bbsnum[5];
    int64_t BbChipID = 0;
    
    getRandNum(bbnonce, noncelen, 256);
    getRandNum(bbsnum, 4, 256);
    
    int n=0; for (int i=1; i<7; i++) BbChipID += (arc4random() % 10) * pow(10, ++n);
    
    
    plist_dict_set_item(parameters, "BbNonce", plist_new_data(bbnonce, noncelen));
    plist_dict_set_item(parameters, "BbChipID", plist_new_uint(BbChipID));
    plist_dict_set_item(parameters, "BbGoldCertId", plist_new_uint(BbGoldCertId));
    plist_dict_set_item(parameters, "BbSNUM", plist_new_data(bbsnum, 4));
    
    /* BasebandFirmware */
    plist_t BasebandFirmware = plist_access_path(tssparameters, 2, "Manifest", "BasebandFirmware");
    if (!BasebandFirmware || plist_get_node_type(BasebandFirmware) != PLIST_DICT) {
        error("ERROR: Unable to get BasebandFirmware node\n");
        return -1;
    }
    plist_t bbfwdict = plist_copy(BasebandFirmware);
    BasebandFirmware = NULL;
    if (plist_dict_get_item(bbfwdict, "Info")) {
        plist_dict_remove_item(bbfwdict, "Info");
    }
    plist_dict_set_item(tssreq, "BasebandFirmware", bbfwdict);
    
    tss_request_add_baseband_tags(tssreq, parameters, NULL);
    return 0;
}

int tss_populate_random(plist_t tssreq, int is64bit){
    uint64_t ecid = 0;
    char nonce[noncelen+1];
    char sep_nonce[noncelen+1];
    
    int n=0;
    for (int i=0; i<16; i++) ecid += (arc4random() % 10) * pow(10, n++);
    getRandNum(nonce, noncelen, 256);
    getRandNum(sep_nonce, noncelen, 256);
    
    nonce[noncelen] = sep_nonce[noncelen] = 0;
    
    debug("[TSSR] ecid=%llu\n",ecid);
    debug("[TSSR] nonce=%s\n",nonce);
    debug("[TSSR] sepnonce=%s\n",sep_nonce);
    
    return tss_populate_devicevals(tssreq, ecid, nonce, noncelen, sep_nonce, noncelen, is64bit);
}



int tssrequest(plist_t *tssrequest, char *buildManifest, int64_t BbGoldCertId){
#define reterror(a) {error(a); error = -1; goto error;}
    int error = 0;
    plist_t manifest = NULL;
    plist_t tssparameter = plist_new_dict();
    plist_t tssreq = tss_request_new(NULL);
    
    plist_from_xml(buildManifest, (unsigned)strlen(buildManifest), &manifest);
    
    plist_t buildidentities = plist_dict_get_item(manifest, "BuildIdentities");
    if (!buildidentities || plist_get_node_type(buildidentities) != PLIST_ARRAY){
        reterror("[TSSR] Error: could not get BuildIdentities\n");
    }
    plist_t id0 = plist_array_get_item(buildidentities, 0);
    if (!id0 || plist_get_node_type(id0) != PLIST_DICT){
        reterror("[TSSR] Error: could not get id0\n");
    }
    plist_t manifestdict = plist_dict_get_item(id0, "Manifest");
    if (!manifestdict || plist_get_node_type(manifestdict) != PLIST_DICT){
        reterror("[TSSR] Error: could not get manifest\n");
    }
    plist_t sep = plist_dict_get_item(manifestdict, "SEP");
    int is64Bit = !(!sep || plist_get_node_type(sep) != PLIST_DICT);
    
    tss_populate_random(tssparameter,is64Bit);
    tss_parameters_add_from_manifest(tssparameter, id0);
    
    debug_plist(tssparameter);
    
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
    
    
#warning DEBUG
    if (BbGoldCertId) {
        tss_populate_basebandvals(tssreq,tssparameter,BbGoldCertId);
        tss_request_add_baseband_tags(tssreq, tssparameter, NULL);
    }
    
    
    *tssrequest = tssreq;
error:
    plist_free(manifest);
    plist_free(tssparameter);
    if (error) plist_free(tssreq);
    return error;
#undef reterror
}

int isManifestSigned(char *buildManifest, int64_t BbGoldCertId){
    
    plist_t tssreq = NULL;
    
#warning DEBUG is64bit
    tssrequest(&tssreq,buildManifest,BbGoldCertId);
    
    plist_t apticket = tss_request_send(tssreq, NULL);
    
    debug_plist(apticket);
    
    if (tssreq) plist_free(tssreq);
    return 1;
}

#pragma mark print functions

int printListOfDevices(char *firmwarejson, jsmntok_t *tokens){
    
    log("[JSON] printing device list\n");
    jsmntok_t *ctok = objectForKey(tokens, firmwarejson, "devices");
    for (jsmntok_t *tmp = ctok->value; ; tmp = tmp->next) {
        printJString(tmp, firmwarejson);
        if (tmp->next == ctok->value) break;
    }
    return 0;
}

int printListOfIPSWForDevice(char *firmwarejson, jsmntok_t *tokens, char *device){
    
    log("[JSON] printing ipsw list for device %s\n",device);
    jsmntok_t *devices = objectForKey(tokens, firmwarejson, "devices");
    jsmntok_t *mydevice = objectForKey(devices, firmwarejson, device);
    jsmntok_t *firmwares = objectForKey(mydevice, firmwarejson, "firmwares");
    
    for (jsmntok_t *tmp = firmwares->value; tmp != NULL; tmp = tmp->next) {
        jsmntok_t *ios = objectForKey(tmp, firmwarejson, "version");
        printJString(ios->value, firmwarejson);
        
    }
    return 0;
}

int printListOfOTAForDevice(char *firmwarejson, jsmntok_t *tokens, char *device){
    
    log("[JSON] printing ota list for device %s\n",device);
    jsmntok_t *mydevice = objectForKey(tokens, firmwarejson, device);
    jsmntok_t *firmwares = objectForKey(mydevice, firmwarejson, "firmwares");
    
    for (jsmntok_t *tmp = firmwares->value; tmp != NULL; tmp = tmp->next) {
        jsmntok_t *ios = objectForKey(tmp, firmwarejson, "version");
        printJString(ios->value, firmwarejson);
        
    }
    return 0;
}
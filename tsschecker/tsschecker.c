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
#include "tsschecker.h"
#include "jsmn.h"
#include "download.h"
#include <libpartialzip-1.0/libpartialzip.h>

#define FIRMWARE_JSON_PATH "/tmp/firmware.json"
//#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json"
#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json/condensed"
#define FIRMWARE_OTA_JSON_PATH "/tmp/ota.json"
#define FIRMWARE_OTA_JSON_URL "https://api.ipsw.me/v2.1/ota.json/condensed"

#define MANIFEST_SAVE_PATH "/tmp/tsschecker"

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
        if (downloadPartialzip(url, (isOta) ? "BuildManifest.plist" : "AssetData/boot/BuildManifest.plist", fileDir)){
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
    fclose(f);
    
    
    free(fileDir);
    return buildmanifest;
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
//
//  ipswme.h
//  tsschecker
//
//  Created by tihmstar on 07.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#ifndef tsscheker_h
#define tsscheker_h

#include <stdio.h>
#include "jsmn.h"
#include <plist/plist.h>

char *getFirmwareJson();
char *getOtaJson();
int parseTokens(char *json, jsmntok_t **tokens);
int printListOfDevices(char *firmwarejson, jsmntok_t *tokens);
int printListOfiOSForDevice(char *firmwarejson, jsmntok_t *tokens, char *device, int isOTA);

char *getFirmwareUrl(char *device, char *version,char *firmwarejson, jsmntok_t *tokens, int isOta);
char *getBuildManifest(char *url, int isOta);
int64_t getBBGCIDForDevice(char *deviceModel);

int tssrequest(plist_t *tssrequest, char *buildManifest, char *device);
int isManifestSignedForDevice(char *buildManifest, char *device);
int isVersionSignedForDevice(char *firmwareJson, jsmntok_t *firmwareTokens, char *version, char *device, int otaFirmware, int checkBaseband);

int checkDeviceExists(char *device, char *firmwareJson, jsmntok_t *tokens, int isOta);
int checkFirmwareForDeviceExists(char *device, char *version, char *firmwareJson, jsmntok_t *tokens, int isOta);

#endif /* tsscheker_h */

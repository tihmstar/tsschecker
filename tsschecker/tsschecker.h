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

char *getFirmwareJson();
char *getOtaJson();
int parseTokens(char *json, jsmntok_t **tokens);
int printListOfDevices(char *firmwarejson, jsmntok_t *tokens);
int printListOfIPSWForDevice(char *firmwarejson, jsmntok_t *tokens, char *device);
int printListOfOTAForDevice(char *firmwarejson, jsmntok_t *tokens, char *device);

char *getFirmwareUrl(char *device, char *version,char *firmwarejson, jsmntok_t *tokens, int isOta);
char *getBuildManifest(char *url, int isOta);


#endif /* tsscheker_h */

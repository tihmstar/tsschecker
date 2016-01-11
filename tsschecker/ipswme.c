//
//  ipswme.c
//  tsschecker
//
//  Created by tihmstar on 07.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include "ipswme.h"
#include "jsmn.h"
#include <string.h>
#include <stdlib.h>

int nextKey(jsmntok_t *tokens){
    int ret = 1;
    if (tokens->type == JSMN_ARRAY || tokens->type == JSMN_OBJECT) {
        for (int i=0; i<tokens->size; i++) {
            ret += nextKey(tokens+ret);
        }
    }
    
    return ret;
}

void printJString(jsmntok_t *str, char * firmwarejson){
    if (str->type != JSMN_STRING) {
        printf("ERROR printing jsmn_string!\n");
        return;
    }
    for (int j=0; j<str->end-str->start; j++) {
        putchar(*(firmwarejson+str->start + j));
    }
    putchar('\n');
}

jsmntok_t *parseTokens(char *json){
    jsmn_parser parser;
    jsmn_init(&parser);
    
    jsmntok_t *tokens;
    printf("[JSON] counting emlements\n");
    unsigned int tokensCnt = jsmn_parse(&parser, json, strlen(json), NULL, 0);
    
    tokens = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokensCnt);
    jsmn_init(&parser);
    printf("[JSON] parsing emlements\n");
    size_t ret = jsmn_parse(&parser, json, strlen(json), tokens, tokensCnt);
    return (ret>0) ? tokens : (jsmntok_t*)ret;
}

int getListOfDevices(char *firmwarejson, char **retList, size_t *retcnt){
    
    jsmntok_t *tokens = parseTokens(firmwarejson);
    if (tokens<=0) {
        printf("[JSON] ERROR parsing json failed\n");
        return -1;
    }
    
    printf("[JSON] generating device list\n");
    jsmntok_t *ctok = tokens->value->value;
    for (jsmntok_t *tmp = ctok; ; tmp = tmp->next) {
        printJString(tmp, firmwarejson);
        if (tmp->next == ctok) break;
    }
    free(tokens);
    return 0;
}
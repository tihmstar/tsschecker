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

int getListOfDevices(char *firmwarejson, char **retList, size_t *retcnt){
    jsmn_parser parser;
    jsmn_init(&parser);
    
    jsmntok_t *tokens;
    printf("[JSON] counting emlements\n");
    size_t tokensCnt = jsmn_parse(&parser, firmwarejson, strlen(firmwarejson), NULL, NULL);
    
    tokens = (jsmntok_t*)malloc(sizeof(jsmntok_t) * tokensCnt);
    jsmn_init(&parser);
    printf("[JSON] parsing emlements\n");
    size_t ret = jsmn_parse(&parser, firmwarejson, strlen(firmwarejson), tokens, tokensCnt);
    
    
    printf("[JSON] generating device list\n");
    jsmntok_t *ctok = tokens->value->value;
    for (jsmntok_t *tmp = ctok->value; ; tmp = tmp->next) {
        printJString(tmp, firmwarejson);
        
        if (tmp->next == ctok->value) break;
    }
    
    
    
    
    return 0;
}
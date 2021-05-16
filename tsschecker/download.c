//
//  download.c
//  tsschecker
//
//  Created by tihmstar on 07.01.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include <curl/curl.h>
#include "download.h"

size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
    return fwrite(buffer, size, nmemb, stream);
}

int downloadFile(const char *url, const char *dstPath){
    info("[DOWN] downloading file %s\n",url);
    CURL *mcurl = curl_easy_init();
    
    FILE *dfile = fopen(dstPath, "wb");
    
    curl_easy_setopt(mcurl, CURLOPT_URL, url);
    curl_easy_setopt(mcurl, CURLOPT_TIMEOUT, 20L); //20 sec
    curl_easy_setopt(mcurl, CURLOPT_WRITEFUNCTION, my_fwrite);
    curl_easy_setopt(mcurl, CURLOPT_WRITEDATA, dfile);
    curl_easy_setopt(mcurl, CURLOPT_FAILONERROR, 1);
    
    CURLcode res = curl_easy_perform(mcurl);
    curl_easy_cleanup(mcurl);
    fclose(dfile);
    if (res != CURLE_OK){
        error("failed to download file from=%s to=%s CURLcode=%d\n",url,dstPath,res);
        return res;
    }
    return 0;
}

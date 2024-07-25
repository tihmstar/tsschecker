//
//  FirmwareAPI_IPSWME.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI_IPSWME.hpp>
#include <tsschecker/tsschecker.hpp>

#include <libgeneral/macros.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

extern "C"{
#include "../external/jssy/jssy/jssy.h"
}

using namespace tihmstar::tsschecker;

#define FIRMWARE_JSON_URL "https://api.ipsw.me/v2.1/firmwares.json/condensed"
#define FIRMWARE_OTA_JSON_URL "https://api.ipsw.me/v2.1/ota.json/condensed"

FirmwareAPI_IPSWME::FirmwareAPI_IPSWME(bool ota)
: _ota(ota), _tokens(NULL)
{
    
}

FirmwareAPI_IPSWME::~FirmwareAPI_IPSWME(){
    safeFree(_tokens);
}

#pragma mark public

void FirmwareAPI_IPSWME::load(){
    long tokencnt = 0;
    _buf = tsschecker::downloadFile(_ota ? FIRMWARE_OTA_JSON_URL : FIRMWARE_JSON_URL);
    retassure((tokencnt = jssy_parse((const char*)_buf.data(), _buf.size(), NULL, 0)) > 0, "Failed to parse json");
    safeFree(_tokens);
    _tokens = (jssytok_t*)calloc(1, tokencnt * sizeof(jssytok_t));
    retassure((tokencnt = jssy_parse((const char*)_buf.data(), _buf.size(), _tokens, tokencnt * sizeof(jssytok_t))) > 1, "Failed to parse json");
}

void FirmwareAPI_IPSWME::loadcache(){
    int fd = -1;
    cleanup([&]{
        safeClose(fd);
    });
    struct stat st = {};
    long tokencnt = 0;

    auto cachepath = getCachePath() + (_ota ? "firmwares.json" : "ota.json");
    retassure((fd = open(cachepath.c_str(), O_RDONLY)), "Failed to read file '%s'",cachepath.c_str());
    retassure(!fstat(fd, &st), "Failed to stat file");
    _buf.resize(st.st_size);
    retassure(read(fd, _buf.data(), _buf.size()) == _buf.size(), "Failed to read from file");

    retassure((tokencnt = jssy_parse((const char*)_buf.data(), _buf.size(), NULL, 0)) > 0, "Failed to parse json");
    safeFree(_tokens);
    _tokens = (jssytok_t*)calloc(1, tokencnt * sizeof(jssytok_t));
    retassure((tokencnt = jssy_parse((const char*)_buf.data(), _buf.size(), _tokens, tokencnt * sizeof(jssytok_t))) > 0, "Failed to parse json");
}

void FirmwareAPI_IPSWME::storecache(){
    int fd = -1;
    cleanup([&]{
        safeClose(fd);
    });
    auto cachepath = getCachePath() + (!_ota ? "firmwares.json" : "ota.json");
    debug("Storing cache at %s",cachepath.c_str());
    retassure((fd = open(cachepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755)), "Failed to create file '%s'",cachepath.c_str());
    retassure(write(fd, _buf.data(), _buf.size()) == _buf.size(), "Failed to write to file");
}

std::vector<std::string> FirmwareAPI_IPSWME::listDevices(){
    if (_devicesCache.size() == 0) {
        jssytok_t *devs = NULL;
        retassure(devs = jssy_dictGetValueForKey(_tokens, "devices"), "Failed to get devices key");
        {
            retassure(devs->type == JSSY_DICT || devs->type == JSSY_ARRAY, "devices not DICT or ARRAY");
            size_t i=0;
            for (jssytok_t *t=devs->subval; i<devs->size; t=t->next,i++) {
                _devicesCache.push_back({t->value,t->value+t->size});
            }
        }
        std::sort(_devicesCache.begin(), _devicesCache.end(), [](const auto &a, const auto &b)->int{
            const char *aa = a.c_str();
            const char *bb = b.c_str();
            while (true) {
                char ca = tolower(*aa++);
                char cb = tolower(*bb++);
                if ((ca | cb) == 0) return 0;
                if (isalpha(ca) || ca == ','){
                    if (int diff = ca-cb) return diff < 0;
                    else continue;
                }
                int na = atoi(&aa[-1]);
                int nb = atoi(&bb[-1]);
                while (isdigit(*aa)) aa++;
                while (isdigit(*bb)) bb++;
                if (na == nb) continue;
                return na > nb;
            }
        });
        _versionsCache.clear();
    }
    return _devicesCache;
}

std::vector<FirmwareAPI::firmwareVersion> FirmwareAPI_IPSWME::listVersionsForDevice(std::string device){
    if (_versionsCache.find(device) == _versionsCache.end()) {
        std::vector<firmwareVersion> ret;
        jssytok_t *devs = NULL;
        if (!(devs = jssy_dictGetValueForKey(_tokens, "devices"))){
            debug("Failed to get devices key. Assuming this is ota.json and using root as fallback");
            devs = _tokens;
        }
        {
            retassure(devs->type == JSSY_DICT || devs->type == JSSY_ARRAY, "devices not DICT or ARRAY");
            size_t i=0;
            for (jssytok_t *t=devs->subval; i<devs->size; t=t->next,i++) {
                if (device == std::string{t->value,t->value+t->size}) {
                    jssytok_t *firmwares = NULL;
                    retassure(firmwares = jssy_dictGetValueForKey(t->subval, "firmwares"), "Failed to get firmwares key");
                    retassure(firmwares->type == JSSY_ARRAY, "firmwares not ARRAY");
                    size_t j=0;
                    for (jssytok_t *tt=firmwares->subval; j<firmwares->size; tt=tt->next,j++) {
                        jssytok_t *buildid = NULL;
                        jssytok_t *version = NULL;
                        jssytok_t *url = NULL;
                        retassure(buildid = jssy_dictGetValueForKey(tt, "buildid"), "Failed to get buildid key");
                        retassure(version = jssy_dictGetValueForKey(tt, "version"), "Failed to get version key");
                        retassure(url = jssy_dictGetValueForKey(tt, "url"), "Failed to get version key");
                        ret.push_back({
                            .version = std::string{version->value,version->value+version->size},
                            .build = std::string{buildid->value,buildid->value+buildid->size},
                            .url = std::string{url->value,url->value+url->size},
                        });
                    }
                    break;
                }
            }
        }
        std::sort(ret.begin(), ret.end(),[](const auto &a, const auto &b)->int{
            const char *aa = a.build.c_str();
            const char *bb = b.build.c_str();
            while (true) {
                char ca = tolower(*aa++);
                char cb = tolower(*bb++);
                if ((ca | cb) == 0) return 0;
                if (isalpha(ca) || ca == ','){
                    if (int diff = ca-cb) return diff > 0;
                    else continue;
                }
                int na = atoi(&aa[-1]);
                int nb = atoi(&bb[-1]);
                while (isdigit(*aa)) aa++;
                while (isdigit(*bb)) bb++;
                if (na == nb) continue;
                return na > nb;
            }
        });
        if (ret.size()) _versionsCache[device] = ret;
    }

    try {
        return _versionsCache.at(device);
    } catch (...) {
        debug("No versions found for device '%s'",device.c_str());
        return {};
    }
}


FirmwareAPI::firmwareVersion FirmwareAPI_IPSWME::getURLForDeviceAndBuild(uint32_t cpid, uint32_t bdid, std::string version, std::string build){
    std::string device = tsschecker::getProductTypeFromCPIDandBDID(cpid, bdid);
    auto firmwares = listVersionsForDevice(device);
    retassure(firmwares.size(), "no firmwares found for device '%s'",device.c_str());
    transform(build.begin(), build.end(), build.begin(), ::tolower);
    if (build.size() || version.size()) {
        for (auto f : firmwares) {
            transform(f.build.begin(), f.build.end(), f.build.begin(), ::tolower);
            if (build.size() && f.build == build) return f;
            else if (f.version == version) return f;
        }
        reterror("Failed to find firmware '%s' '%s'",version.c_str(),build.c_str());
    }else{
        return firmwares.front();
    }
}

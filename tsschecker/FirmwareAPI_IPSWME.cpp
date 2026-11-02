//
//  FirmwareAPI_IPSWME.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/FirmwareAPI_IPSWME.hpp>
#include <tsschecker/tsschecker.hpp>

#include <libgeneral/macros.h>
#include <libgeneral/Mem.hpp>
#include <libgeneral/Utils.hpp>
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <strings.h>

extern "C"{
#include "../external/jssy/jssy/jssy.h"
}

using namespace tihmstar::tsschecker;

#define IPSWME_API_URL "https://api.ipsw.me/v4/"

struct FirmwareAPI_IPSWME::JsonDoc{
    tihmstar::Mem buf;
    std::vector<jssytok_t> tokens; //tokens point into buf

    JsonDoc(tihmstar::Mem data) : buf(std::move(data)){
        long tokencnt = 0;
        retassure((tokencnt = jssy_parse((const char*)buf.data(), buf.size(), NULL, 0)) > 1, "Failed to parse json");
        tokens.resize(tokencnt);
        retassure(jssy_parse((const char*)buf.data(), buf.size(), tokens.data(), tokens.size() * sizeof(jssytok_t)) > 1, "Failed to parse json");
    }
};

static std::string jsonString(const jssytok_t *t){
    if (!t || t->type != JSSY_STRING) return {};
    return {t->value, t->value + t->size};
}

static std::string cacheFileNameForEndpoint(std::string endpoint){
    for (auto &c : endpoint) {
        if (!isalnum((unsigned char)c) && c != '.' && c != '-') c = '_';
    }
    return endpoint + ".json";
}

FirmwareAPI_IPSWME::FirmwareAPI_IPSWME(bool ota, bool beta)
: _ota(ota), _beta(beta), _readCache(false)
{
    
}

FirmwareAPI_IPSWME::~FirmwareAPI_IPSWME(){
    //
}

#pragma mark private
const jssytok_t *FirmwareAPI_IPSWME::fetch(const std::string &endpoint){
    auto &doc = _responses[endpoint];
    if (!doc) {
        tihmstar::Mem buf;
        bool fromCache = false;
        if (_readCache) {
            auto cachepath = getCachePath() + cacheFileNameForEndpoint(endpoint);
            try {
                buf = tihmstar::readFile(cachepath.c_str());
                fromCache = true;
                debug("Using cached '%s'",cachepath.c_str());
            } catch (tihmstar::exception &e) {
                debug("No cache for '%s'",endpoint.c_str());
            }
        }
        if (!fromCache) buf = tsschecker::downloadFile((IPSWME_API_URL + endpoint).c_str());
        doc = std::make_unique<JsonDoc>(std::move(buf));
    }
    return doc->tokens.data();
}

#pragma mark public
void FirmwareAPI_IPSWME::load(){
    fetch("devices");
}

void FirmwareAPI_IPSWME::loadcache(){
    _readCache = true;
    fetch("devices");
}

void FirmwareAPI_IPSWME::storecache(){
    for (auto &r : _responses) {
        if (!r.second) continue;
        auto cachepath = getCachePath() + cacheFileNameForEndpoint(r.first);
        debug("Storing cache at %s",cachepath.c_str());
        tihmstar::writeFile(cachepath.c_str(), r.second->buf.data(), r.second->buf.size());
    }
}

std::vector<std::string> FirmwareAPI_IPSWME::listDevices(){
    if (_devicesCache.size() == 0) {
        const jssytok_t *devs = fetch("devices");
        retassure(devs->type == JSSY_ARRAY, "devices not ARRAY");
        size_t i=0;
        for (jssytok_t *t=devs->subval; i<devs->size; t=t->next,i++) {
            auto identifier = jsonString(jssy_dictGetValueForKey(t, "identifier"));
            if (identifier.size()) _devicesCache.push_back(identifier);
        }
        std::sort(_devicesCache.begin(), _devicesCache.end(), [](const auto &a, const auto &b)->int{
            const char *aa = a.c_str();
            const char *bb = b.c_str();
            while (true) {
                char ca = tolower((unsigned char)*aa++);
                char cb = tolower((unsigned char)*bb++);
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
    }
    return _devicesCache;
}

std::vector<FirmwareAPI::firmwareVersion> FirmwareAPI_IPSWME::listVersionsForDevice(std::string device){
    if (_versionsCache.find(device) == _versionsCache.end()) {
        std::vector<firmwareVersion> fulls;
        std::vector<firmwareVersion> deltas;
        const jssytok_t *dev = fetch("device/" + device + (_ota ? "?type=ota" : "?type=ipsw"));
        if (jssytok_t *firmwares = jssy_dictGetValueForKey(dev, "firmwares")) {
            retassure(firmwares->type == JSSY_ARRAY, "firmwares not ARRAY");
            size_t j=0;
            for (jssytok_t *tt=firmwares->subval; j<firmwares->size; tt=tt->next,j++) {
                firmwareVersion f = {
                    .version = jsonString(jssy_dictGetValueForKey(tt, "version")),
                    .build = jsonString(jssy_dictGetValueForKey(tt, "buildid")),
                    .url = jsonString(jssy_dictGetValueForKey(tt, "url")),
                };
                if (!f.version.size() || !f.build.size() || !f.url.size()) continue;
                if (!_beta && jsonString(jssy_dictGetValueForKey(tt, "releasetype")) == "Beta") continue;
                //full OTAs list their version as 9.9.<version>
                if (_ota && f.version.rfind("9.9.", 0) == 0) f.version = f.version.substr(4);
                if (jsonString(jssy_dictGetValueForKey(tt, "prerequisitebuildid")).size()) deltas.push_back(f);
                else fulls.push_back(f);
            }
        }
        //one entry per build, full OTAs before deltas
        std::vector<firmwareVersion> ret;
        for (auto list : {&fulls, &deltas}) {
            for (auto &f : *list) {
                if (std::none_of(ret.begin(), ret.end(), [&](const firmwareVersion &r){return r.build == f.build;})) ret.push_back(f);
            }
        }

        std::sort(ret.begin(), ret.end(),[](const auto &a, const auto &b)->int{
            const char *aa = a.build.c_str();
            const char *bb = b.build.c_str();
            while (true) {
                char ca = tolower((unsigned char)*aa++);
                char cb = tolower((unsigned char)*bb++);
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



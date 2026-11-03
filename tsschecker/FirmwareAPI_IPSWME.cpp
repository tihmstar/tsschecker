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
#include "../external/jssy/include/jssy/jssy.h"
}

using namespace tihmstar::tsschecker;

#define IPSWME_API_URL "https://api.ipsw.me/v4/"

FirmwareAPI_IPSWME::FirmwareAPI_IPSWME(bool ota, bool beta)
: _ota(ota), _beta(beta)
{
    
}

FirmwareAPI_IPSWME::~FirmwareAPI_IPSWME(){
    //
}

#pragma mark public
void FirmwareAPI_IPSWME::load(){
    fetch(IPSWME_API_URL "devices");
}

std::vector<std::string> FirmwareAPI_IPSWME::listDevices(){
    if (_devicesCache.size() == 0) {
        const jssytok_t *devs = fetch(IPSWME_API_URL "devices");
        retassure(devs->type == JSSY_ARRAY, "devices not ARRAY");
        size_t i=0;
        for (jssytok_t *t=devs->subval; i<devs->size; t=t->next,i++) {
            auto identifier = jsonString(jssy_dictGetValueForKey(t, "identifier"));
            if (identifier.size()) _devicesCache.push_back(identifier);
        }
        sortDevices(_devicesCache);
    }
    return _devicesCache;
}

std::vector<FirmwareAPI::firmwareVersion> FirmwareAPI_IPSWME::listVersionsForDevice(std::string device){
    if (_versionsCache.find(device) == _versionsCache.end()) {
        std::vector<firmwareVersion> fulls;
        std::vector<firmwareVersion> deltas;
        const jssytok_t *dev = fetch(IPSWME_API_URL "device/" + device + (_ota ? "?type=ota" : "?type=ipsw"));
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

        sortFirmwaresByBuild(ret);
        if (ret.size()) _versionsCache[device] = ret;
    }

    try {
        return _versionsCache.at(device);
    } catch (...) {
        debug("No versions found for device '%s'",device.c_str());
        return {};
    }
}



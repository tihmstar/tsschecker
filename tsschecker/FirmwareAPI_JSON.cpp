//
//  FirmwareAPI_JSON.cpp
//  tsschecker
//

#include <tsschecker/FirmwareAPI_JSON.hpp>
#include <tsschecker/tsschecker.hpp>

#include <libgeneral/macros.h>
#include <libgeneral/Mem.hpp>
#include <libgeneral/Utils.hpp>
#include <algorithm>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern "C"{
#include "../external/jssy/jssy/jssy.h"
}

using namespace tihmstar::tsschecker;

struct FirmwareAPI_JSON::JsonDoc{
    tihmstar::Mem buf;
    std::vector<jssytok_t> tokens; //tokens point into buf, parsed on first fetch()

    JsonDoc(tihmstar::Mem data) : buf(std::move(data)){}

    void parse(){
        long tokencnt = 0;
        retassure((tokencnt = jssy_parse((const char*)buf.data(), buf.size(), NULL, 0)) > 1, "Failed to parse json");
        std::vector<jssytok_t> t(tokencnt);
        retassure(jssy_parse((const char*)buf.data(), buf.size(), t.data(), t.size() * sizeof(jssytok_t)) > 1, "Failed to parse json");
        tokens = std::move(t);
    }
};

static std::string cacheFileNameForURL(std::string url){
    size_t pos = url.find("://");
    if (pos != std::string::npos) url = url.substr(pos+3);
    for (auto &c : url) {
        if (!isalnum((unsigned char)c) && c != '.' && c != '-') c = '_';
    }
    if (url.size() < 5 || url.compare(url.size()-5, 5, ".json") != 0) url += ".json";
    return url;
}

//compares strings with embedded numbers numerically, letters case-insensitively. Returns <0, 0, >0
static int compareNatural(const std::string &a, const std::string &b, bool numbersDescending = false){
    const char *aa = a.c_str();
    const char *bb = b.c_str();
    while (true) {
        unsigned char ca = (unsigned char)*aa;
        unsigned char cb = (unsigned char)*bb;
        if (!ca || !cb) return (int)ca - (int)cb;
        if (isdigit(ca) && isdigit(cb)) {
            long na = strtol(aa, (char**)&aa, 10);
            long nb = strtol(bb, (char**)&bb, 10);
            if (na != nb) return (na < nb) != numbersDescending ? -1 : 1;
            continue;
        }
        if (int diff = tolower(ca) - tolower(cb)) return diff;
        aa++; bb++;
    }
}

FirmwareAPI_JSON::FirmwareAPI_JSON()
: _readCache(false)
{

}

FirmwareAPI_JSON::~FirmwareAPI_JSON(){
    //
}

#pragma mark protected
const tihmstar::Mem &FirmwareAPI_JSON::fetchData(const std::string &url){
    auto &doc = _responses[url];
    if (!doc) {
        tihmstar::Mem buf;
        bool fromCache = false;
        if (_readCache) {
            auto cachepath = getCachePath() + cacheFileNameForURL(url);
            try {
                buf = tihmstar::readFile(cachepath.c_str());
                fromCache = true;
                debug("Using cached '%s'",cachepath.c_str());
            } catch (tihmstar::exception &e) {
                debug("No cache for '%s'",url.c_str());
            }
        }
        try {
            if (!fromCache) buf = tsschecker::downloadFile(url.c_str());
        } catch (...) {
            _responses.erase(url);
            throw;
        }
        doc = std::make_unique<JsonDoc>(std::move(buf));
    }
    return doc->buf;
}

const jssytok_t *FirmwareAPI_JSON::fetch(const std::string &url){
    fetchData(url);
    auto &doc = _responses.at(url);
    if (doc->tokens.empty()) {
        try {
            doc->parse();
        } catch (tihmstar::exception &e) {
            _responses.erase(url);
            reterror("Failed to parse json from '%s'",url.c_str());
        }
    }
    return doc->tokens.data();
}

std::string FirmwareAPI_JSON::jsonString(const jssytok_t *t){
    if (!t || t->type != JSSY_STRING) return {};
    return {t->value, t->value + t->size};
}

bool FirmwareAPI_JSON::jsonBool(const jssytok_t *t){
    return t && t->type == JSSY_PRIMITIVE && t->size == 4 && strncmp(t->value, "true", 4) == 0;
}

void FirmwareAPI_JSON::sortDevices(std::vector<std::string> &devices){
    std::sort(devices.begin(), devices.end(), [](const std::string &a, const std::string &b){
        return compareNatural(a, b, true) < 0;
    });
}

void FirmwareAPI_JSON::sortFirmwaresByBuild(std::vector<firmwareVersion> &firmwares){
    std::stable_sort(firmwares.begin(), firmwares.end(), [](const firmwareVersion &a, const firmwareVersion &b){
        return compareNatural(a.build, b.build) > 0;
    });
}

#pragma mark public
void FirmwareAPI_JSON::loadcache(){
    _readCache = true;
    load();
}

void FirmwareAPI_JSON::storecache(){
    for (auto &r : _responses) {
        if (!r.second) continue;
        auto cachepath = getCachePath() + cacheFileNameForURL(r.first);
        debug("Storing cache at %s",cachepath.c_str());
        tihmstar::writeFile(cachepath.c_str(), r.second->buf.data(), r.second->buf.size());
    }
}

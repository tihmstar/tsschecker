//
//  FirmwareAPI_AppleDB.cpp
//  tsschecker
//

#include <tsschecker/FirmwareAPI_AppleDB.hpp>
#include <tsschecker/tsschecker.hpp>

#include <libgeneral/macros.h>
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <strings.h>

using namespace tihmstar::tsschecker;

#define APPLEDB_API_URL "https://api.appledb.dev/"

static const char *osPathForDevice(const std::string &device){
    const char *d = device.c_str();
    if (!strncasecmp(d, "Watch", 5)) return "watchOS";
    if (!strncasecmp(d, "iPhone", 6) || !strncasecmp(d, "iPad", 4) || !strncasecmp(d, "iPod", 4)) return "iOS";
    if (!strncasecmp(d, "AppleTV", 7)) return "tvOS";
    if (!strncasecmp(d, "AudioAccessory", 14)) return "HomePod%20Software";
    if (!strncasecmp(d, "iBridge", 7)) return "bridgeOS";
    if (!strncasecmp(d, "RealityDevice", 13)) return "visionOS";
    if (!strncasecmp(d, "Mac", 3) || !strncasecmp(d, "iMac", 4) || !strncasecmp(d, "VirtualMac", 10)) return "macOS";
    return NULL;
}

template <typename F>
static void forEachArrayItem(plist_t arr, F f){
    if (!arr || plist_get_node_type(arr) != PLIST_ARRAY) return;
    uint32_t cnt = plist_array_get_size(arr);
    for (uint32_t i=0; i<cnt; i++) f(plist_array_get_item(arr, i));
}

static std::string getString(plist_t dict, const char *key){
    plist_t v = plist_dict_get_item(dict, key);
    if (!v || plist_get_node_type(v) != PLIST_STRING) return {};
    uint64_t len = 0;
    const char *s = plist_get_string_ptr(v, &len);
    return {s, s + len};
}

static bool getBool(plist_t dict, const char *key){
    return plist_bool_val_is_true(plist_dict_get_item(dict, key));
}

static bool arrayContainsString(plist_t arr, const std::string &str){
    bool ret = false;
    forEachArrayItem(arr, [&](plist_t t){
        if (ret || plist_get_node_type(t) != PLIST_STRING) return;
        uint64_t len = 0;
        const char *s = plist_get_string_ptr(t, &len);
        ret = (len == str.size() && strncasecmp(s, str.c_str(), len) == 0);
    });
    return ret;
}

static bool endsWith(const std::string &s, const char *suffix){
    size_t len = strlen(suffix);
    return s.size() >= len && strcasecmp(s.c_str() + s.size() - len, suffix) == 0;
}

//picks the preferred active link of a source. Encrypted Apple Archives (.aea) are skipped:
//libfragmentzip can't read the BuildManifest out of them
static std::string urlForSource(plist_t source, bool *skippedAEA){
    std::string ret;
    bool done = false;
    forEachArrayItem(plist_dict_get_item(source, "links"), [&](plist_t link){
        if (done) return;
        std::string url = getString(link, "url");
        if (!url.size()) return;
        if (endsWith(url, ".aea")) {
            if (skippedAEA) *skippedAEA = true;
            return;
        }
        if (plist_dict_get_item(link, "active") && !getBool(link, "active")) return;
        bool isPreferred = getBool(link, "preferred");
        if (!ret.size() || isPreferred) ret = url;
        if (isPreferred) done = true;
    });
    return ret;
}

//URL of the IPSW (or full OTA, falling back to a delta OTA) for device in a build record
static std::string urlForDeviceInBuild(plist_t build, const std::string &device, const char *type, bool *skippedAEA = NULL){
    std::string full;
    std::string delta;
    forEachArrayItem(plist_dict_get_item(build, "sources"), [&](plist_t source){
        if (getString(source, "type") != type) return;
        if (!arrayContainsString(plist_dict_get_item(source, "deviceMap"), device)) return;
        std::string &dst = plist_dict_get_item(source, "prerequisiteBuild") ? delta : full;
        if (!dst.size()) dst = urlForSource(source, skippedAEA);
    });
    return full.size() ? full : delta;
}

FirmwareAPI_AppleDB::FirmwareAPI_AppleDB(bool ota, bool beta)
: _ota(ota), _beta(beta)
{

}

FirmwareAPI_AppleDB::~FirmwareAPI_AppleDB(){
    for (auto &d : _docs) safeFreeCustom(d.second, plist_free);
}

#pragma mark private
plist_t FirmwareAPI_AppleDB::fetchPlist(const std::string &url, plist_type type){
    auto &doc = _docs[url];
    if (!doc) {
        //parsed with libplist: AppleDB has empty arrays followed by more keys, which jssy can't parse
        const tihmstar::Mem &buf = fetchData(url);
        if (plist_from_json((const char*)buf.data(), (uint32_t)buf.size(), &doc) != PLIST_ERR_SUCCESS) safeFreeCustom(doc, plist_free);
        if (doc && plist_get_node_type(doc) != type) safeFreeCustom(doc, plist_free);
        if (!doc) _docs.erase(url);
        retassure(doc, "Failed to parse json from '%s'",url.c_str());
    }
    return doc;
}

#pragma mark protected
bool FirmwareAPI_AppleDB::resolveFirmwareURL(firmwareVersion &f, const std::string &device){
    if (f.url.size()) return true;
    if (!f.key.size()) return false;
    std::string key = f.key;
    for (size_t pos = 0; (pos = key.find(' ', pos)) != std::string::npos;) key.replace(pos, 1, "%20");
    plist_t build = fetchPlist(APPLEDB_API_URL "ios/" + key + ".json", PLIST_DICT);
    bool skippedAEA = false;
    f.url = urlForDeviceInBuild(build, device, _ota ? "ota" : "ipsw", &skippedAEA);
    //stop here instead of falling back to an older build that happens to have a readable file
    retassure(f.url.size() || !skippedAEA, "%s %s for %s is only available as encrypted .aea, which can't be read. Use -m with its BuildManifest.plist",f.version.c_str(),f.build.c_str(),device.c_str());
    return f.url.size() != 0;
}

#pragma mark public
void FirmwareAPI_AppleDB::load(){
    //catalogs are per OS and large, they are fetched once the device is known
}

std::vector<std::string> FirmwareAPI_AppleDB::listDevices(){
    if (_devicesCache.size() == 0) {
        plist_t devs = fetchPlist(APPLEDB_API_URL "device/main.json", PLIST_ARRAY);
        forEachArrayItem(devs, [&](plist_t dev){
            //only devices with an AP that TSS signs for
            if (!plist_dict_get_item(dev, "cpid")) return;
            forEachArrayItem(plist_dict_get_item(dev, "identifier"), [&](plist_t t){
                if (plist_get_node_type(t) != PLIST_STRING) return;
                std::string identifier = plist_get_string_ptr(t, NULL);
                if (!identifier.size() || !osPathForDevice(identifier)) return;
                if (std::find(_devicesCache.begin(), _devicesCache.end(), identifier) == _devicesCache.end()) _devicesCache.push_back(identifier);
            });
        });
        sortDevices(_devicesCache);
    }
    return _devicesCache;
}

std::vector<FirmwareAPI::firmwareVersion> FirmwareAPI_AppleDB::listVersionsForDevice(std::string device){
    if (_versionsCache.find(device) == _versionsCache.end()) {
        const char *os = osPathForDevice(device);
        retassure(os, "AppleDB: unknown OS for device '%s'",device.c_str());
        plist_t catalog = fetchPlist(std::string(APPLEDB_API_URL "ios/") + os + "/main.json", PLIST_ARRAY);

        std::vector<firmwareVersion> ret;
        forEachArrayItem(catalog, [&](plist_t build){
            if (!arrayContainsString(plist_dict_get_item(build, "deviceMap"), device)) return;
            if (getBool(build, "internal")) return;
            if (!_beta && (getBool(build, "beta") || getBool(build, "rc"))) return;
            firmwareVersion f = {
                .version = getString(build, "version"),
                .build = getString(build, "build"),
            };
            //"18.4 RC 2" -> "18.4"
            f.version = f.version.substr(0, f.version.find_first_not_of("0123456789."));
            if (!f.version.size() || !f.build.size()) return;
            if (std::any_of(ret.begin(), ret.end(), [&](const firmwareVersion &r){return r.build == f.build;})) return;
            if (_ota) {
                //the catalog has no OTA links, resolveFirmwareURL looks them up in the build record
                f.key = getString(build, "key");
                if (!f.key.size()) return;
            }else{
                f.url = urlForDeviceInBuild(build, device, "ipsw");
                if (!f.url.size()) return;
            }
            ret.push_back(f);
        });
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

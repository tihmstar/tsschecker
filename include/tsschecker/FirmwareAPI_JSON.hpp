//
//  FirmwareAPI_JSON.hpp
//  tsschecker
//

#ifndef FirmwareAPI_JSON_hpp
#define FirmwareAPI_JSON_hpp

#include <tsschecker/FirmwareAPI.hpp>
#include <libgeneral/Mem.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct jssytok;
namespace tihmstar{
namespace tsschecker {
//shared base for firmware lists served as JSON: downloads, parses and caches responses per URL
class FirmwareAPI_JSON : public FirmwareAPI {
    struct JsonDoc;
    std::map<std::string,std::unique_ptr<JsonDoc>> _responses; //URL -> parsed response
protected:
    bool _readCache;

    //raw response body, downloaded once per URL (or read from the cache)
    const tihmstar::Mem &fetchData(const std::string &url);
    //response parsed with jssy (jssy rejects an empty array or dict that has a sibling after it)
    const jssytok *fetch(const std::string &url);

    static std::string jsonString(const jssytok *t);
    static bool jsonBool(const jssytok *t);
    //sorts device identifiers like iPhone10,1 numerically
    static void sortDevices(std::vector<std::string> &devices);
    //sorts firmwares newest build first
    static void sortFirmwaresByBuild(std::vector<firmwareVersion> &firmwares);
public:
    FirmwareAPI_JSON();
    virtual ~FirmwareAPI_JSON() override;

    virtual void loadcache() override;
    virtual void storecache() override;
};
};
};

#endif /* FirmwareAPI_JSON_hpp */

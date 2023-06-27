//
//  TssRequest.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef TssRequest_hpp
#define TssRequest_hpp

#include <stdint.h>
#include <plist/plist.h>
#include <iostream>
#include <vector>

#define RESTORE_VARIANT_ERASE_INSTALL               "Erase Install (IPSW)"
#define RESTORE_VARIANT_UPGRADE_INSTALL             "Upgrade Install (IPSW)"
#define RESTORE_VARIANT_RESEARCH_ERASE_INSTALL      "Research Customer Erase Install (IPSW)"
#define RESTORE_VARIANT_RESEARCH_UPGRADE_INSTALL    "Research Customer Upgrade Install (IPSW)"
#define RESTORE_VARIANT_MACOS_RECOVERY_OS           "macOS Customer"

namespace tihmstar {
namespace tsschecker {
class TssRequest{
    plist_t _pBuildManifest;
    plist_t _pBuildIdentity;
    plist_t _pReq;

    std::string _variant;
    uint64_t _generator;
    
    void setStandardValues();
public:
    TssRequest(const plist_t pBuildManifest, std::string variant = "", bool isBuildIdentity = false);
    ~TssRequest();
    
    plist_t getTSSResponce();
    
#pragma mark verifiers
    bool isProductTypeValidForRequest(const char *productType);
    
#pragma mark value modifiers
    void setDeviceVals(uint32_t cpid, uint32_t bdid, bool force = false);
    void setEcid(uint64_t ecid);
    void setNonceGenerator(uint64_t generator);
    void setAPNonce(std::vector<uint8_t> nonce);
    void setSEPNonce(std::vector<uint8_t> nonce);
    void setRandomSEPNonce();
    
    //baseband
    void setBbGoldCertId(uint64_t bbgoldcertid);
    void setBbGoldCertIdForDevice(const char *productType);
    void setDefaultBbGoldCertId();
    void setSNUM(std::vector<uint8_t> snum);
    void setRandomSNUM();

    //advances
    bool addComponent(const char *componentName, bool optional = false);
    bool deleteComponent(const char *componentName);
    void unsetAPNonce();
    
#pragma mark value getters
    uint32_t getCPID() const;
    uint32_t getBDID() const;
    uint64_t getECID() const;
    uint64_t getGenerator() const;
    std::string getProductType() const;
    std::vector<uint8_t> getAPNonce() const;
    std::string getAPNonceString() const;

    std::string getProductVersion() const;
    std::string getBuildVersion() const;

    plist_t getSelectedBuildIdentity();
    
#pragma mark configuration specifiers
    void addDefaultAPTagsToRequest();
    void addAllAPComponentsToRequest();
    void addBasebandComponentsToRequest();
    void addYonkersComponentsToRequest();

    void removeBasebandComponentsFromRequest();
    void removeNonBasebandComponentsFromRequest();

#pragma mark debugging
    void dumpRequest();
    
#pragma mark static
    static plist_t getBuildIdentityForDevice(plist_t pBuildManifest, uint32_t cpid, uint32_t bdid, std::string variant = "");
    static std::string getVariantNameFromBuildIdentity(plist_t pBuildIdentity);
    static std::string getPathForComponentBuildIdentity(plist_t pBuildIdentity, const char *component);
    static plist_t getElementForComponentBuildIdentity(plist_t pBuildIdentity, const char *component);

    static void applyRestoreRulesForManifestComponent(plist_t component, plist_t restoreRules, plist_t tss_request);
    static void copyKeyFromPlist(plist_t request, plist_t manifest, const char *key, bool optional = false);
    static uint64_t getNumberFromStringElementInDict(plist_t dict, const char *key, int base = 16);
    static std::vector<uint8_t> getApImg4TicketFromTssResponse(plist_t tssrsp);
    static std::vector<uint8_t> getElementFromTssResponse(plist_t tssrsp, const char *key);

    static char *TssSendPlistRequest(const plist_t tssreq, const char *server_url_string = NULL);
    static char *TssSendRawBuffer(const char *buf, size_t bufSize, const char *server_url_string = NULL);
};
};
};

#endif /* TssRequest_hpp */

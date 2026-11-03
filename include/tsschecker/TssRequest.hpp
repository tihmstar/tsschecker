//
//  TssRequest.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef TssRequest_hpp
#define TssRequest_hpp

#include <libgeneral/Mem.hpp>
#include <plist/plist.h>
#include <iostream>
#include <vector>
#include <initializer_list>
#include <stdint.h>

#define RESTORE_VARIANT_ERASE_INSTALL               "Customer Erase Install (IPSW)"
#define RESTORE_VARIANT_UPGRADE_INSTALL             "Customer Upgrade Install (IPSW)"
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
    std::string _productType;
    uint64_t _generator;
    
    void setStandardValues();
    plist_t createRequestEntryForManifestComponent(plist_t pComponent);
    //resets the request to a bare one and returns the parameters (manifest values, then device values) like libtatsu
    plist_t beginCoprocessorRequest(plist_t deviceValues, std::initializer_list<const char*> manifestKeys, bool mergeDeviceValues = true);
    void mergeDeviceGeneratedRequest(plist_t deviceValues);
public:
    TssRequest(const plist_t pBuildManifest, std::string variant = "", bool isBuildIdentity = false);
    TssRequest(const TssRequest &) = delete;
    TssRequest &operator=(const TssRequest &) = delete;
    ~TssRequest();
    
    plist_t getTSSResponce(const char *serverURL = NULL);
    
#pragma mark verifiers
    bool isProductTypeValidForRequest(const char *productType);
    
#pragma mark value modifiers
    void setDeviceVals(uint32_t cpid, uint32_t bdid, bool force = false, const char *productType = NULL);
    void setEcid(uint64_t ecid);
    bool takesAPNonce() const;
    void setNonceGenerator(uint64_t generator);
    void setAPNonce(const void *nonceData, size_t nonceSize);
    void setAPNonce(const tihmstar::Mem &nonce);
    void setSEPNonce(const void *nonceData, size_t nonceSize);
    void setSEPNonce(const tihmstar::Mem &nonce);
    void setRandomSEPNonce();
    
    //baseband
    void setBbGoldCertId(uint64_t bbgoldcertid);
    void setBbGoldCertIdForDevice(const char *productType);
    void setDefaultBbGoldCertId();
    void setSNUM(const tihmstar::Mem &snum);
    void setRandomSNUM();

    //advances
    bool addComponent(const char *componentName, bool optional = false);
    bool deleteComponent(const char *componentName);
    void unsetAPNonce();
    //nonce-less ticket ("noNonce" in saved blobs): TSS needs ApNonce present but empty, status 69 without it
    void setEmptyAPNonce();
    
#pragma mark value getters
    uint32_t getCPID() const;
    uint32_t getBDID() const;
    uint64_t getECID() const;
    uint64_t getGenerator() const;
    std::string getProductType() const;
    tihmstar::Mem getAPNonce() const;
    std::string getAPNonceString() const;

    std::string getProductVersion() const;
    std::string getBuildVersion() const;

    plist_t getSelectedBuildIdentity();
    
#pragma mark configuration specifiers
    void addDefaultAPTagsToRequest(bool skipOptional = false);
    void addAllAPComponentsToRequest(bool skipNoniBootComponents = false);
    void addBasebandComponentsToRequest();
    //turns this request into a separate Cryptex1 ticket request (call after setDeviceVals and setEcid)
    // iOS 27 adds two explicit request fields idevicerestore/libauthinstall set that
    // older libtatsu did not (RE'd from the on-device libauthinstall.dylib):
    //   * Cryptex1,NonceDomain — fourcc tag selecting which img4 nonce domain the
    //     ticket binds to (e.g. "trca" for cryptex1.boot.os, "trcs" for ...app).
    //     An empty string omits the key for backward compatibility with iOS 26-.
    //   * Cryptex1,ChipID — mirror of ApChipID; iOS 27 TSS reads it even though
    //     Cryptex1,UDID already encodes it.
    void makeCryptex1Request(const tihmstar::Mem &cryptexNonce,
                             const std::string &nonceDomain = "");
    //turn this request into a separate co-processor ticket request (libtatsu tss_request_add_{se,savage,yonkers,rose}_tags).
    //deviceValues holds what the device reports during a restore, with libtatsu's key names, and optionally the
    //device's DeviceGeneratedRequest. Call after setDeviceVals
    void makeSERequest(plist_t deviceValues);
    void makeSavageRequest(plist_t deviceValues);
    void makeYonkersRequest(plist_t deviceValues);
    void makeRoseRequest(plist_t deviceValues);

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
    static tihmstar::Mem getApImg4TicketFromTssResponse(plist_t tssrsp);
    static tihmstar::Mem getElementFromTssResponse(plist_t tssrsp, const char *key);

    static char *TssSendPlistRequest(const plist_t tssreq, const char *server_url_string = NULL);
    static char *TssSendRawBuffer(const char *buf, size_t bufSize, const char *server_url_string = NULL);
};
};
};

#endif /* TssRequest_hpp */

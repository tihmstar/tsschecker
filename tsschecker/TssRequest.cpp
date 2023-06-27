//
//  TssRequest.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/TssRequest.hpp>
#include <tsschecker/tsschecker.hpp>
#include <tsschecker/TSSException.hpp>
#include <libgeneral/macros.h>
#include <curl/curl.h>
#include <time.h>
#include <string.h>

#ifdef HAVE_OPENSSL
#   include <openssl/sha.h>
#else
#   ifdef HAVE_COMMCRYPTO
#       include <CommonCrypto/CommonDigest.h>
#       define SHA1(d, n, md) CC_SHA1(d, n, md)
#       define SHA384(d, n, md) CC_SHA384(d, n, md)
#       define SHA_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#       define SHA384_DIGEST_LENGTH CC_SHA384_DIGEST_LENGTH
#   endif //HAVE_COMMCRYPTO
#endif // HAVE_OPENSSL

using namespace tihmstar::tsschecker;
#define TSS_CLIENT_VERSION_STRING "libauthinstall-698.0.5"


#define TSS_MAX_TRIES 10

#pragma mark helpers
#ifndef NO_GENERATE_GUID
static char *generate_guid(){
#define GET_RAND(min, max) ((rand() % (max - min)) + min)
    char *guid = (char *)malloc(sizeof(char) * 37);
    const char *chars = "ABCDEF0123456789";
    srand((unsigned int)time(NULL));
    int i = 0;
    
    for (i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            guid[i] = '-';
            continue;
        } else {
            guid[i] = chars[GET_RAND(0, 16)];
        }
    }
    guid[36] = '\0';
    return guid;
#undef GET_RAND
}
#endif

#pragma mark TSSRequest
TssRequest::TssRequest(const plist_t pBuildManifest, std::string variant, bool isBuildIdentity)
: _pBuildManifest(!isBuildIdentity ? plist_copy((plist_t)pBuildManifest) : NULL), _pBuildIdentity(isBuildIdentity ? plist_copy((plist_t)pBuildManifest) : NULL), _pReq(NULL)
, _variant(variant), _generator(0)
{
    retassure(_pBuildManifest || _pBuildIdentity, "Failed to copy buildmanifest");
    retassure(_pReq = plist_new_dict(), "Failed to create new request");

    setStandardValues();
}

TssRequest::~TssRequest(){
    safeFreeCustom(_pReq, plist_free);
    safeFreeCustom(_pBuildIdentity, plist_free);
    safeFreeCustom(_pBuildManifest, plist_free);
}

#pragma mark private
void TssRequest::setStandardValues(){
    plist_dict_set_item(_pReq, "@Locality", plist_new_string("en_US"));
    plist_dict_set_item(_pReq, "@HostPlatformInfo",
#if defined(WIN32)
        plist_new_string("windows")
#elif defined(__APPLE__)
        plist_new_string("mac")
#elif defined(LINUX)
        plist_new_string("linux")
#else
        plist_new_string("unknown")
#endif
    );
    plist_dict_set_item(_pReq, "@VersionInfo", plist_new_string(TSS_CLIENT_VERSION_STRING));
    {
        char* guid = NULL;
        cleanup([&]{
            safeFree(guid);
        });
        
        if ((guid = generate_guid())) {
            plist_dict_set_item(_pReq, "@UUID", plist_new_string(guid));
        }
    }
    plist_dict_set_item(_pReq, "ApProductionMode", plist_new_bool(1));
}

#pragma mark public
plist_t TssRequest::getTSSResponce(){
    char *rsp = NULL;
    cleanup([&]{
        safeFree(rsp);
    });
    retcustomassure(TSSException_NoTicket, rsp = TssSendPlistRequest(_pReq), "Failed to get ticket");
    {
        plist_t ret = NULL;
        plist_from_memory(rsp, (uint32_t)strlen(rsp), &ret, NULL);
        if (ret) {
            if (_generator) {
                char gbuf[100] = {};
                snprintf(gbuf, sizeof(gbuf), "0x%llx",_generator);
                plist_dict_set_item(ret, "generator", plist_new_string(gbuf));
            }
        }
        return ret;
    }
}

#pragma mark verifiers
bool TssRequest::isProductTypeValidForRequest(const char *productType){
    plist_array_iter p_iter_SupportedProductType = NULL;
    cleanup([&]{
        safeFree(p_iter_SupportedProductType);
    });
    plist_t pSupportedProductType = NULL;
    plist_t pProductType = NULL;

    if (_pBuildManifest) {
        retcustomassure(TSSException_unsupportedProductType, pSupportedProductType = plist_dict_get_item(_pBuildManifest, "SupportedProductTypes"), "Failed to get SupportedProductTypes");
        plist_array_new_iter(pSupportedProductType, &p_iter_SupportedProductType);
        for (plist_array_next_item(pSupportedProductType, p_iter_SupportedProductType, &pProductType); pProductType; plist_array_next_item(pSupportedProductType, p_iter_SupportedProductType, &pProductType)) {
            if (plist_string_val_compare(pProductType, productType) == 0) return true;
        }
    }else if (_pBuildIdentity){
        bool isInvalid = false;
        isInvalid |= (tsschecker::getCPIDForProductType(productType) != getNumberFromStringElementInDict(_pBuildIdentity, "ApChipID"));
        isInvalid |= (tsschecker::getBDIDForProductType(productType) != getNumberFromStringElementInDict(_pBuildIdentity, "ApBoardID"));
        return !isInvalid;
    }else{
        reterror("Unexpecte not having neither BuildManifest nor Buildidentity");
    }
    return false;
}
    
#pragma mark value modifiers
void TssRequest::setDeviceVals(uint32_t cpid, uint32_t bdid, bool force){
    const char *productType = NULL;
    try{
        //check whether the ProductType is supported
        productType = tsschecker::getProductTypeFromCPIDandBDID(cpid, bdid);
        retcustomassure(TSSException_unsupportedProductType, isProductTypeValidForRequest(productType), "ProductType is not valid for this request");
    }catch (tihmstar::TSSException_unsupportedProductType &e){
        if (!force) throw;
        warning("Ignoring incompatible ProductType.\n%s",e.dumpStr().c_str());
    }

    info("Setting device vals: %s CPID:0x%x BDID:0x%x",productType,cpid,bdid);
    plist_dict_set_item(_pReq, "ApChipID", plist_new_uint(cpid));
    plist_dict_set_item(_pReq, "ApBoardID", plist_new_uint(bdid));
}

void TssRequest::setEcid(uint64_t ecid){
    info("Setting ecid:0x%llx",ecid);
    plist_dict_set_item(_pReq, "ApECID", plist_new_uint(ecid));
}

void TssRequest::setNonceGenerator(uint64_t generator){
    _generator = generator;
    auto nonceType = tsschecker::nonceTypeForCPID(getCPID());
    if (nonceType != kNonceTypeNone){
        uint8_t *apnonce = NULL;
        cleanup([&]{
            safeFree(apnonce);
        });
        size_t apnonceLen = 0;
        switch (nonceType) {
            case kNonceTypeSHA1:
                apnonceLen = SHA_DIGEST_LENGTH;
                retassure(apnonce = (uint8_t*)malloc(SHA_DIGEST_LENGTH), "failed to malloc(%d)",SHA_DIGEST_LENGTH);
                SHA1((const unsigned char*)&generator,sizeof(generator),apnonce);
                plist_dict_set_item(_pReq, "ApNonce", plist_new_data((char*)apnonce, apnonceLen));
                break;
                
            case kNonceTypeSHA384:
                apnonceLen = 32;
                retassure(apnonce = (uint8_t*)malloc(SHA384_DIGEST_LENGTH), "failed to malloc(%d)",SHA384_DIGEST_LENGTH);
                SHA384((const unsigned char*)&generator,sizeof(generator),apnonce);
                plist_dict_set_item(_pReq, "ApNonce", plist_new_data((char*)apnonce, apnonceLen));
                break;
                
            default:
                reterror("Unexpected nonce type=%d",nonceType);
                break;
        }
    }
}

void TssRequest::setAPNonce(std::vector<uint8_t> nonce){
    auto nonceType = tsschecker::nonceTypeForCPID(getCPID());
    retassure(nonce.size() == 20 || nonceType != kNonceTypeSHA1, "Noncetype is SHA1 but noncelen is %d (expecting %d)",nonce.size(),20);
    retassure(nonce.size() == 32 || nonceType != kNonceTypeSHA384, "Noncetype is SHA384 but noncelen is %d (expecting %d)",nonce.size(),32);
    plist_dict_set_item(_pReq, "ApNonce", plist_new_data((char*)nonce.data(), nonce.size()));
}

void TssRequest::setSEPNonce(std::vector<uint8_t> nonce){
    plist_dict_set_item(_pReq, "SepNonce", plist_new_data((char*)nonce.data(), nonce.size()));
}

void TssRequest::setRandomSEPNonce(){
    std::vector<uint8_t> nonce;
    nonce.resize(SHA_DIGEST_LENGTH);
    srand((unsigned int)time(NULL));
    uint64_t sgen = ((uint64_t)rand() << 32) | rand();
    SHA1((const unsigned char*)&sgen, 8, nonce.data());
    setSEPNonce(nonce);
}

void TssRequest::setBbGoldCertId(uint64_t bbgoldcertid){
    plist_dict_set_item(_pReq, "BbGoldCertId", plist_new_uint(bbgoldcertid));
}

void TssRequest::setBbGoldCertIdForDevice(const char *productType){
    setBbGoldCertId(getGoldCertIDForDevice(productType));
}

void TssRequest::setDefaultBbGoldCertId(){
    setBbGoldCertIdForDevice(getProductTypeFromCPIDandBDID(getCPID(), getBDID()));
}

void TssRequest::setSNUM(std::vector<uint8_t> snum){
    auto snumsize = getSNUMLenForDevice(getProductTypeFromCPIDandBDID(getCPID(), getBDID()));
    retassure(snum.size() == snumsize, "Bad BbSNUM len %d (expecting %d)",snum.size(),snumsize);
    plist_dict_set_item(_pReq, "BbSNUM", plist_new_data((char*)snum.data(), snum.size()));
}

void TssRequest::setRandomSNUM(){
    auto snumsize = getSNUMLenForDevice(getProductTypeFromCPIDandBDID(getCPID(), getBDID()));
    std::vector<uint8_t> snum;
    srand((unsigned int)time(NULL));
    while (snum.size() < snumsize) snum.push_back((uint8_t)rand());
    setSNUM(snum);
}

bool TssRequest::addComponent(const char *componentName, bool optional){
    plist_t pIdentity = getSelectedBuildIdentity();
    plist_t pComponent = NULL;

    if ((pComponent = plist_dict_get_item(pIdentity, componentName))) {
        //components in BuildIdentity like UniqueBuildID
        plist_dict_set_item(_pReq, componentName, plist_copy(pComponent));
        return true;
    }else{
        //components inside Manifest
        pComponent = plist_dict_get_item(plist_dict_get_item(pIdentity, "Manifest"), componentName);
        retassure(pComponent || optional, "Failed to add custom non-optional component '%s'",componentName);
        if (pComponent){
            //add element to request
            plist_t pKey = NULL;
            cleanup([&]{
                safeFreeCustom(pKey, plist_free);
            });
            pKey = plist_new_dict();
            iterateOverPlistElementsInDict(pComponent, [&](const char *key, plist_t e)->void*{
                if (plist_get_node_type(e) == PLIST_DICT) return NULL;
                plist_dict_set_item(pKey, key, plist_copy(e));
                return NULL;
            });

            if (plist_t pInfo = plist_dict_get_item(pComponent, "Info")) {
                if (plist_t pRestoreRules = plist_dict_get_item(pInfo, "RestoreRequestRules")) {
                    applyRestoreRulesForManifestComponent(pKey, pRestoreRules, _pReq);
                }
            }
            plist_dict_set_item(_pReq, componentName, pKey); pKey = NULL;
            info("Adding custom component '%s' to request for variant '%s'",componentName,getVariantNameFromBuildIdentity(pIdentity).c_str());
            return true;
        }else{
            info("Not adding custom component '%s' to request for variant '%s'",componentName,getVariantNameFromBuildIdentity(pIdentity).c_str());
            return false;
        }
    }
}

bool TssRequest::deleteComponent(const char *componentName){
    bool ret = !!plist_dict_get_item(_pReq, componentName);
    plist_dict_remove_item(_pReq, componentName);
    return ret;
}

void TssRequest::unsetAPNonce(){
    deleteComponent("ApNonce");
}

#pragma mark value getters
uint32_t TssRequest::getCPID() const{
    plist_t pVal = NULL;
    retassureMissingValue("ApChipID", pVal = plist_dict_get_item(_pReq, "ApChipID"), "ApChipID not set");
    retassureMissingValue("ApChipID", plist_get_node_type(pVal) == PLIST_UINT, "ApChipID not UINT");
    {
        uint64_t ret = 0;
        plist_get_uint_val(pVal, &ret);
        return (uint32_t)ret;
    }
}

uint32_t TssRequest::getBDID() const{
    plist_t pVal = NULL;
    retassureMissingValue("ApBoardID", pVal = plist_dict_get_item(_pReq, "ApBoardID"), "ApBoardID not set");
    retassureMissingValue("ApBoardID", plist_get_node_type(pVal) == PLIST_UINT, "ApBoardID not UINT");
    {
        uint64_t ret = 0;
        plist_get_uint_val(pVal, &ret);
        return (uint32_t)ret;
    }
}

uint64_t TssRequest::getECID() const{
    plist_t pVal = NULL;
    retassureMissingValue("ApECID", pVal = plist_dict_get_item(_pReq, "ApECID"), "ApECID not set");
    retassureMissingValue("ApECID", plist_get_node_type(pVal) == PLIST_UINT, "ApECID not UINT");
    {
        uint64_t ret = 0;
        plist_get_uint_val(pVal, &ret);
        return (uint32_t)ret;
    }
}

uint64_t TssRequest::getGenerator() const{
    return _generator;
}

std::string TssRequest::getProductType() const{
    return getProductTypeFromCPIDandBDID(getCPID(), getBDID());
}

std::vector<uint8_t> TssRequest::getAPNonce() const{
    plist_t pVal = NULL;
    retassureMissingValue("ApNonce", pVal = plist_dict_get_item(_pReq, "ApNonce"), "ApNonce not set");
    retassureMissingValue("ApNonce", plist_get_node_type(pVal) == PLIST_DATA, "ApNonce not DATA");
    {
        const char *s = NULL;
        uint64_t slen = 0;
        retassureMissingValue("ApNonce", s = plist_get_data_ptr(pVal, &slen), "Failed to get DATA");
        return {(uint8_t*)s,(uint8_t*)s+slen};
    }
}

std::string TssRequest::getAPNonceString() const{
    std::string ret;
    for (auto v : getAPNonce()){
        char buf[4] = {};
        snprintf(buf, sizeof(buf), "%02x",v);
        ret += buf;
    }
    return ret;
}

std::string TssRequest::getProductVersion() const{
    plist_t pVal = NULL;
    retassureMissingValue("ProductVersion", pVal = plist_dict_get_item(_pBuildManifest, "ProductVersion"), "ProductVersion not set");
    retassureMissingValue("ProductVersion", plist_get_node_type(pVal) == PLIST_STRING, "ProductVersion not STRING");
    {
        const char *s = NULL;
        uint64_t slen = 0;
        retassureMissingValue("ProductVersion", s = plist_get_string_ptr(pVal, &slen), "Failed to get ProductVersion");
        return {s,s+slen};
    }
}

std::string TssRequest::getBuildVersion() const{
    plist_t pVal = NULL;
    retassureMissingValue("ProductBuildVersion", pVal = plist_dict_get_item(_pBuildManifest, "ProductBuildVersion"), "ProductBuildVersion not set");
    retassureMissingValue("ProductBuildVersion", plist_get_node_type(pVal) == PLIST_STRING, "ProductBuildVersion not STRING");
    {
        const char *s = NULL;
        uint64_t slen = 0;
        retassureMissingValue("ProductBuildVersion", s = plist_get_string_ptr(pVal, &slen), "Failed to get ProductBuildVersion");
        return {s,s+slen};
    }
}


plist_t TssRequest::getSelectedBuildIdentity(){
    if (_pBuildIdentity) return _pBuildIdentity;
    plist_t pIdentity = NULL;
    if (!_variant.size()){
        try {
            if ((pIdentity = getBuildIdentityForDevice(_pBuildManifest, getCPID(), getBDID(),RESTORE_VARIANT_ERASE_INSTALL))){
                _variant = RESTORE_VARIANT_ERASE_INSTALL;
                debug("Implicitly setting variant to '%s'",_variant.c_str());
            }
        } catch (...) {
            //
        }
    }
    if (!pIdentity) pIdentity = getBuildIdentityForDevice(_pBuildManifest, getCPID(), getBDID(),_variant);
    retassure(pIdentity, "Failed to get buildidentity with variant '%s'",_variant.c_str());
    return pIdentity;
}

#pragma mark configuration specifiers
void TssRequest::addDefaultAPTagsToRequest(){
    plist_t pIdentity = getSelectedBuildIdentity();
    info("Adding default AP Tags to request for variant '%s'",getVariantNameFromBuildIdentity(pIdentity).c_str());
    plist_dict_set_item(_pReq, "ApSecurityDomain", plist_new_uint(getNumberFromStringElementInDict(pIdentity,"ApSecurityDomain")));
    plist_dict_set_item(_pReq, "ApSecurityMode", plist_new_bool(1));

    bool hasPartialDigest = (bool)iterateOverPlistElementsInDict(plist_dict_get_item(pIdentity, "Manifest"), [&](const char *key, plist_t e)->void*{
        return plist_dict_get_item(e, "PartialDigest");
    });

    bool requiresUIDMode = (bool)iterateOverPlistElementsInDict(plist_dict_get_item(pIdentity, "Info"), [&](const char *key, plist_t e)->void*{
        if (strcmp(key, "RequiresUIDMode") == 0) return (void*)(uint64_t)plist_bool_val_is_true(e);
        return NULL;
    });

    if (requiresUIDMode) {
        plist_dict_set_item(_pReq, "UID_MODE", plist_new_bool(0));
    }
    
    if (!hasPartialDigest) {
        //this is IMG4
        plist_dict_set_item(_pReq, "@ApImg4Ticket", plist_new_bool(1));
        
        //IMG4 always needs SEP nonce!
        if (!plist_dict_get_item(_pReq, "SepNonce")) setRandomSEPNonce();
    }else{
        //this is IMG3
        plist_dict_set_item(_pReq, "@APTicket", plist_new_bool(1));
#warning TODO add support for iOS 4 without APTickets
    }
}

void TssRequest::addAllAPComponentsToRequest(){
    plist_t pIdentity = getSelectedBuildIdentity();
    info("Adding all AP components to request for variant '%s'",getVariantNameFromBuildIdentity(pIdentity).c_str());
    
    copyKeyFromPlist(_pReq, pIdentity, "UniqueBuildID");
    plist_t pManifest = NULL;
    retassure(pManifest = plist_dict_get_item(pIdentity, "Manifest"), "Failed to get Manifest");
    
    iterateOverPlistElementsInDict(pManifest, [&](const char *key, plist_t e)->void*{
        //add element to request
        plist_t pKey = NULL;
        cleanup([&]{
            safeFreeCustom(pKey, plist_free);
        });
        pKey = plist_new_dict();
        iterateOverPlistElementsInDict(e, [&](const char *key, plist_t e)->void*{
            if (plist_get_node_type(e) == PLIST_DICT) return NULL;
            plist_dict_set_item(pKey, key, plist_copy(e));
            return NULL;
        });

        if (plist_t pInfo = plist_dict_get_item(e, "Info")) {
            if (plist_t pRestoreRules = plist_dict_get_item(pInfo, "RestoreRequestRules")) {
                applyRestoreRulesForManifestComponent(pKey, pRestoreRules, _pReq);
            }
        }
        bool addComponent = plist_dict_get_item(pKey, "Digest") || plist_dict_get_item(pKey, "PartialDigest");
        if (strncmp(key, "Yonkers", sizeof("Yonkers")-1) == 0) addComponent = false;
        else if (strncmp(key, "Savage", sizeof("Savage")-1) == 0) addComponent = false;
        else if (strncmp(key, "Cryptex", sizeof("Cryptex")-1) == 0) addComponent = false;
        else if (strncmp(key, "BMU", sizeof("BMU")-1) == 0) addComponent = false;
        else if (strncmp(key, "eUICC", sizeof("eUICC")-1) == 0) addComponent = false;

        if (addComponent) {
            debug("Adding component '%s'",key);
            plist_dict_set_item(_pReq, key, pKey); pKey = NULL;
        }else{
            debug("Not adding component '%s'",key);
        }
        return NULL;
    });
}

void TssRequest::addBasebandComponentsToRequest(){
    plist_t pIdentity = getSelectedBuildIdentity();
    info("Adding Baseband components to request for variant '%s'",getVariantNameFromBuildIdentity(pIdentity).c_str());
    copyKeyFromPlist(_pReq, plist_dict_get_item(pIdentity, "Manifest"), "BasebandFirmware");
    iterateOverPlistElementsInDict(pIdentity, [&](const char *key, plist_t e)->void*{
        if (strncmp(key, "Bb", 2) == 0) copyKeyFromPlist(_pReq, pIdentity, key);
        return NULL;
    });
}

void TssRequest::addYonkersComponentsToRequest(){
    reterror("TODO implement!");
//    plist_t pIdentity = getSelectedBuildIdentity();
//    info("Adding Yonkery components to request for variant '%s'",getVariantNameFromBuildIdentity(pIdentity).c_str());
//    
//    plist_t pManifest = NULL;
//    retassure(pManifest = plist_dict_get_item(pIdentity, "Manifest"), "Failed to get Manifest");
//    
//    iterateOverPlistElementsInDict(pManifest, [&](const char *key, plist_t e)->void*{
//        if (strncmp(key, "Yonkers", sizeof("Yonkers")-1) != 0) return NULL;
//        //add element to request
//        plist_t pKey = NULL;
//        cleanup([&]{
//            safeFreeCustom(pKey, plist_free);
//        });
//        pKey = plist_new_dict();
//        iterateOverPlistElementsInDict(e, [&](const char *key, plist_t e)->void*{
//            if (plist_get_node_type(e) == PLIST_DICT) return NULL;
//            plist_dict_set_item(pKey, key, plist_copy(e));
//            return NULL;
//        });
//
//        if (plist_t pInfo = plist_dict_get_item(e, "Info")) {
//            if (plist_t pRestoreRules = plist_dict_get_item(pInfo, "RestoreRequestRules")) {
//                applyRestoreRulesForManifestComponent(pKey, pRestoreRules, _pReq);
//            }
//        }
//        bool addComponent = plist_dict_get_item(pKey, "Digest") || plist_dict_get_item(pKey, "PartialDigest");
//
//        if (addComponent) {
//            debug("Adding component '%s'",key);
//            plist_dict_set_item(_pReq, key, pKey); pKey = NULL;
//        }else{
//            debug("Not adding component '%s'",key);
//        }
//        return NULL;
//    });
//    plist_dict_set_item(_pReq, "@Yonkers,Ticket", plist_new_bool(1));
}


void TssRequest::removeBasebandComponentsFromRequest(){
    std::vector<std::string> delkeys;
    iterateOverPlistElementsInDict(_pReq, [&](const char *key, plist_t e)->void*{
        if (strncmp(key, "Bb", 2) != 0 && strcmp(key, "BasebandFirmware") != 0) return NULL;
        delkeys.push_back(key);
        return NULL;
    });

    for (auto dk : delkeys) {
        plist_dict_remove_item(_pReq, dk.c_str());
    }
}

void TssRequest::removeNonBasebandComponentsFromRequest(){
    std::vector<std::string> delkeys;
    iterateOverPlistElementsInDict(_pReq, [&](const char *key, plist_t e)->void*{
#define whitelistKey(k) if (strcmp(key, k) == 0) return NULL
        //required general keys
        whitelistKey("@Locality");
        whitelistKey("@HostPlatformInfo");
        whitelistKey("@VersionInfo");
        whitelistKey("@UUID");
        whitelistKey("ApChipID");
        whitelistKey("ApProductionMode");

        //actual baseband keys
        whitelistKey("BasebandFirmware");
        if (strncmp(key, "Bb", 2) == 0) return NULL;
        
        delkeys.push_back(key);
#undef whitelistKey
        return NULL;
    });

    for (auto dk : delkeys) {
        plist_dict_remove_item(_pReq, dk.c_str());
    }
}

#pragma mark debugging
void TssRequest::dumpRequest(){
    tsschecker::dumpplist(_pReq);
}

#pragma mark static
plist_t TssRequest::getBuildIdentityForDevice(plist_t pBuildManifest, uint32_t cpid, uint32_t bdid, std::string variant){
    plist_t pBuildIdentities = NULL;
    retassure(pBuildIdentities = plist_dict_get_item(pBuildManifest, "BuildIdentities"), "Failed to get BuildIdentities");
    
    return iterateOverPlistElementsInArray(pBuildIdentities, [&](plist_t e)->plist_t{
        uint64_t cCPID = 0;
        uint64_t cBDID = 0;
        {
            plist_t pCPID = NULL;
            plist_t pBDID = NULL;
            retassure(pCPID = plist_dict_get_item(e, "ApChipID"), "Failed to read cpid of buildidentiy");
            retassure(pBDID = plist_dict_get_item(e, "ApBoardID"), "Failed to read bdid of buildidentiy");

            char *sCPID = NULL;
            char *sBDID = NULL;
            cleanup([&]{
                safeFree(sCPID);
                safeFree(sBDID);
            });
            plist_get_string_val(pCPID, &sCPID);
            plist_get_string_val(pBDID, &sBDID);
            cCPID = strtoll(sCPID, NULL, 16);
            cBDID = strtoll(sBDID, NULL, 16);
         }
        if (cCPID != cpid || cBDID != bdid) return 0;
        
        auto v = getVariantNameFromBuildIdentity(e);
        if (strstr(v.c_str(), variant.c_str())){
            return e;
        }
        return 0;
    });
}

std::string TssRequest::getVariantNameFromBuildIdentity(plist_t pBuildIdentity){
    plist_t pInfo = NULL;
    plist_t pVariant = NULL;
    retassure(pInfo = plist_dict_get_item(pBuildIdentity, "Info"), "Failed to get Info of buildidentiy");
    retassure(pVariant = plist_dict_get_item(pInfo, "Variant"), "Failed to get Variant of buildidentiy Info");

    const char *str = NULL;
    uint64_t strSize = 0;
    retassure(str = plist_get_string_ptr(pVariant, &strSize), "Failed to get variant str");
    return {str,str+strSize};
}

std::string TssRequest::getPathForComponentBuildIdentity(plist_t pBuildIdentity, const char *component){
    plist_t pManifest = NULL;
    plist_t pComponent = NULL;
    plist_t pInfo = NULL;
    plist_t pPath = NULL;
    retassure(pManifest = plist_dict_get_item(pBuildIdentity, "Manifest"), "Failed to get Manifest");
    retassureMissingValue(component, pComponent = plist_dict_get_item(pManifest, component), "Failed to get component in getPathForComponentBuildIdentity");
    retassure(pInfo = plist_dict_get_item(pComponent, "Info"), "Failed to get component Info");
    retassure(pPath = plist_dict_get_item(pInfo, "Path"), "Failed to get component Path");
    {
        const char *s = NULL;
        uint64_t slen = 0;
        retassure(s = plist_get_string_ptr(pPath, &slen), "Failed to get str ptr");
        return {s,s+slen};
    }
}

plist_t TssRequest::getElementForComponentBuildIdentity(plist_t pBuildIdentity, const char *component){
    plist_t pManifest = NULL;
    plist_t pComponent = NULL;
    retassure(pManifest = plist_dict_get_item(pBuildIdentity, "Manifest"), "Failed to get Manifest");
    retassureMissingValue(component, pComponent = plist_dict_get_item(pManifest, component), "Failed to get component in getPathForComponentBuildIdentity");
    return pComponent;
}

void TssRequest::copyKeyFromPlist(plist_t request, plist_t manifest, const char *key, bool optional){
    plist_t pE = NULL;
    retassureMissingValue(key, (pE = plist_dict_get_item(manifest, key)) || optional, "Failed to copyKeyFromPlist");
    if (pE) plist_dict_set_item(request, key, plist_copy(pE));
}

uint64_t TssRequest::getNumberFromStringElementInDict(plist_t dict, const char *key, int base){
    plist_t pE = NULL;
    retassure(pE = plist_dict_get_item(dict, key), "Failed to get '%s'",key);
    const char *s = NULL;
    uint64_t slen = 0;
    retassure(s = plist_get_string_ptr(pE, &slen), "Failed to get '%s' string",key);
    return strtoll(s, NULL, base);
}

std::vector<uint8_t> TssRequest::getApImg4TicketFromTssResponse(plist_t tssrsp){
    std::vector<uint8_t> ret;
    plist_t pApImg4Ticket = NULL;
    retassureMissingValue("ApImg4Ticket", (pApImg4Ticket = plist_dict_get_item(tssrsp, "ApImg4Ticket")), "Failed to get ApImg4Ticket from tssrsp");
    {
        const char *ptr = NULL;
        uint64_t ptrSize = 0;
        retassure(ptr = plist_get_data_ptr(pApImg4Ticket, &ptrSize), "Failed to get data ptr for ApImg4Ticket");
        return {ptr,ptr+ptrSize};
    }
}

std::vector<uint8_t> TssRequest::getElementFromTssResponse(plist_t tssrsp, const char *key){
    std::vector<uint8_t> ret;
    plist_t pApImg4Ticket = NULL;
    retassureMissingValue(key, (pApImg4Ticket = plist_dict_get_item(tssrsp, key)), "Failed to get Element from tssrsp");
    {
        const char *ptr = NULL;
        uint64_t ptrSize = 0;
        retassure(ptr = plist_get_data_ptr(pApImg4Ticket, &ptrSize), "Failed to get data ptr for %s",key);
        return {ptr,ptr+ptrSize};
    }
}

void TssRequest::applyRestoreRulesForManifestComponent(plist_t component, plist_t restoreRules, plist_t tss_request){
    int rule = -1;
    iterateOverPlistElementsInArray(restoreRules, [&](plist_t e)->void*{
        ++rule;
        if (plist_t pConditions = plist_dict_get_item(e, "Conditions")) {
            bool conditionsFailed = !!iterateOverPlistElementsInDict(pConditions, [&](const char *key, plist_t cond)->void*{
                plist_t val = NULL;
                if (strcmp(key, "ApRequiresImage4") == 0)
                    val = plist_dict_get_item(tss_request, "@ApImg4Ticket");
                else if (strcmp(key, "ApRawProductionMode") == 0)
                    val = plist_dict_get_item(tss_request, "ApProductionMode");
                else if (strcmp(key, "ApCurrentProductionMode") == 0)
                    val = plist_dict_get_item(tss_request, "ApProductionMode");
                else if (strcmp(key, "ApDemotionPolicyOverride") == 0)
                    val = plist_dict_get_item(tss_request, "DemotionPolicy");
                else if (strcmp(key, "ApInRomDFU") == 0)
                    val = plist_dict_get_item(tss_request, "ApInRomDFU");
                else if (strcmp(key, "ApRawSecurityMode") == 0)
                    val = plist_dict_get_item(tss_request, "ApSecurityMode");
                else reterror("Unknown restore rule '%s'",key);

                bool ret = false;
                if (val) ret = plist_compare_node_value(val, cond);
//                debug("[%d] Checking rule '%s'=%s [%s]",rule,key,plist_bool_val_is_true(cond) ? "TRUE" : "FALSE",ret ? "PASS" : "FAIL");
                return (void*)!ret;
            });
            if (conditionsFailed) return NULL;
            
            iterateOverPlistElementsInDict(plist_dict_get_item(e, "Actions"), [&](const char *key, plist_t action)->void*{
                plist_dict_set_item(component, key, plist_copy(action));
                return NULL;
            });
        }
        return NULL;
    });
}

char *TssRequest::TssSendPlistRequest(const plist_t tssreq, const char *server_url_string){
    char *xml = NULL;
    cleanup([&]{
        safeFree(xml);
    });
    uint32_t xmlSize = 0;
    plist_to_xml(tssreq, &xml, &xmlSize);
    return TssSendRawBuffer(xml, xmlSize);
}

char *TssRequest::TssSendRawBuffer(const char *buf, size_t bufSize, const char *server_url_string){
#define TSS_URLS_NUM 6
    const char* urls[TSS_URLS_NUM] = {
        "https://gs.apple.com/TSS/controller?action=2",
        "http://gs.apple.com/TSS/controller?action=2",
        "https://17.111.103.65/TSS/controller?action=2",
        "https://17.111.103.15/TSS/controller?action=2",
        "http://17.111.103.65/TSS/controller?action=2",
        "http://17.111.103.15/TSS/controller?action=2",
    };
    
    for (int i = 0; i < TSS_MAX_TRIES; i++) {
        CURL *mcurl = NULL;
        struct curl_slist* header = NULL;
        cleanup([&]{
            safeFreeCustom(header, curl_slist_free_all);
            safeFreeCustom(mcurl, curl_easy_cleanup);
        });
        std::string rsp;
        char curl_error_message[CURL_ERROR_SIZE] = {};
        const char *url = NULL;
        CURLcode cret = {};
        
        if (server_url_string) {
            url = server_url_string;
        }else{
            url = urls[i % TSS_URLS_NUM];
        }
        
        info("Attempt %d request URL set to %s", i, url);
        retassure(mcurl = curl_easy_init(), "Failed to init curl");
        header = curl_slist_append(header, "Cache-Control: no-cache");
        header = curl_slist_append(header, "Content-type: text/xml; charset=\"utf-8\"");
        header = curl_slist_append(header, "Expect:");
        
        /* disable SSL verification to allow download from untrusted https locations */
        curl_easy_setopt(mcurl, CURLOPT_URL, url);
        curl_easy_setopt(mcurl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(mcurl, CURLOPT_ERRORBUFFER, curl_error_message);
        curl_easy_setopt(mcurl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(mcurl, CURLOPT_POSTFIELDS, buf);
        curl_easy_setopt(mcurl, CURLOPT_USERAGENT, "InetURL/1.0");
        curl_easy_setopt(mcurl, CURLOPT_POSTFIELDSIZE, bufSize);
        curl_easy_setopt(mcurl, CURLOPT_WRITEFUNCTION, (size_t (*)(void *, size_t, size_t, void *))[](void *contents, size_t size, size_t nmemb, void *userp)->size_t{
            uint8_t *ptr = (uint8_t*)contents;
            size_t realsize = size * nmemb;
            std::string *mem = (std::string *)userp;;
            mem->insert(mem->end(), ptr,ptr+realsize);
            return realsize;
        });
        curl_easy_setopt(mcurl, CURLOPT_WRITEDATA, (void *)&rsp);

        cret = curl_easy_perform(mcurl);
        if (cret != CURLE_OK){
            error("Failed to send request with err=%d (%s)",cret,curl_error_message);
        }
        const char *rspstr = (char*)rsp.c_str();
        const char *status = NULL;
        const char *message = NULL;
        size_t messageLen = 0;
        int status_code = 0;
        
        if (!(status = strstr(rspstr, "STATUS="))){
            error("Failed to find status in response");
            continue;
        }
        status += sizeof("STATUS=")-1;
        if (!(message = strstr(rspstr, "MESSAGE="))){
            error("Failed to find message in response");
        }else{
            message += sizeof("MESSAGE=")-1;
            if (const char *end = strstr(message, "&")) {
                messageLen = end - message;
            }else{
                messageLen = strlen(message);
            }
        }
        status_code = atoi(status);
        info("Status: %d (%.*s)",status_code,messageLen,message);
        if (status_code) break;
        const char *ticket = NULL;
        retassure(ticket = strstr(message+messageLen, "REQUEST_STRING="), "Failed to find response");
        ticket += sizeof("REQUEST_STRING=")-1;
        return strdup(ticket);
    }
    return NULL;
#undef TSS_URLS_NUM
}

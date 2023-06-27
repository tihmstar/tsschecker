//
//  TSSException.hpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#ifndef TSSException_hpp
#define TSSException_hpp

#include <libgeneral/macros.h>
#include <libgeneral/exception.hpp>

namespace tihmstar {
    class TSSException : public tihmstar::exception {
        using exception::exception;
    };

    //custom exceptions for easier catching
    class TSSException_unsupportedProductType : public TSSException{
        using TSSException::TSSException;
    };

    class TSSException_NoTicket : public TSSException{
        using TSSException::TSSException;
    };

    class TSSException_DBLookupFailed : public TSSException{
        using TSSException::TSSException;
    };

    //extended exceptions
    class TSSException_missingValue : public TSSException{
        std::string _keyname;
    public:
        TSSException_missingValue(const char *keyname, const char *commit_count_str, const char *commit_sha_str, int line, const char *filename, const char *err);
        std::string keyname() const;
    };
};

#define retassureMissingValue(key, cond, errstr) do{ if ((cond) == 0) throw tihmstar::TSSException_missingValue(key, VERSION_COMMIT_COUNT, VERSION_COMMIT_SHA, __LINE__,LIBGENERAL__FILE__,LIBGENERAL_ERRSTR(errstr)); } while(0)

#endif /* TSSException_hpp */

//
//  TSSException.cpp
//  tsschecker
//
//  Created by tihmstar on 28.02.23.
//

#include <tsschecker/TSSException.hpp>

using namespace tihmstar;

TSSException_missingValue::TSSException_missingValue(const char *keyname, const char *commit_count_str, const char *commit_sha_str, int line, const char *filename, const char *err)
: TSSException(commit_count_str,commit_sha_str,line,filename, "Key '%s' missing in dict. %s",keyname,err), _keyname(keyname ? keyname : "")
{
    //
}

std::string TSSException_missingValue::keyname() const{
    return _keyname;
}

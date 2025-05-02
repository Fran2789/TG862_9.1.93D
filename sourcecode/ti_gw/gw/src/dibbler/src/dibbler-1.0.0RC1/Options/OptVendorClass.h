/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * released under GNU GPL v2 only licence
 */

#ifndef OPTVENDORCLASS_H
#define OPTVENDORCLASS_H

#include "OptUserClass.h"
#include <stdint.h>

class TOptVendorClass : public TOptUserClass
{
 public:
    uint32_t Enterprise_id_;

    TOptVendorClass(uint16_t type, const char* buf, unsigned short buf_len, TMsg* parent);
    size_t getSize();
    char * storeSelf( char* buf);
// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    bool isGet_Broadband_DslForumString();
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD

// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
protected:
    std::string Broadband_VendorData;
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
};

#endif /* USERCLASS_H */

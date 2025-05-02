/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 * released under GNU GPL v2 licence
 *
 */

#include "DHCPConst.h"
#include "OptVendorClass.h"
#include "Portable.h"
#include <string.h>

TOptVendorClass::TOptVendorClass(uint16_t type, const char* buf, unsigned short buf_len, TMsg* parent)
    :TOptUserClass(type, parent) {
    if (buf_len < 4) {
	Valid = false;
	return;
    }
    Enterprise_id_ = readUint32(buf);
    buf += sizeof(uint32_t);
    buf_len -= sizeof(uint32_t);
    Valid = parseUserData(buf, buf_len);
// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    if ( Valid == true )
    {
        Broadband_VendorData.assign(buf, buf_len);
    }
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD
}

size_t TOptVendorClass::getSize() {
    return 4 + TOptUserClass::getSize();
}

char * TOptVendorClass::storeSelf(char* buf) {
    buf = writeUint16(buf, OptType);
    buf = writeUint16(buf, getSize() - 4);
    buf = writeUint32(buf, Enterprise_id_);
    return storeUserData(buf);
}

// UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
bool TOptVendorClass::isGet_Broadband_DslForumString()
{
    if (this->Broadband_VendorData.find(BROADBAND_DSL_FORUM_STRING) == std::string::npos)
    {
        return false;
    }

    return true;
}
#endif // INCLUDE_ARRIS_GW_TR69
// END UNIHAN ADD


/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *
 *
 * released under GNU GPL v2 only licence
 */

#ifndef OPTVENDORSPECINFO_H
#define OPTVENDORSPECINFO_H

#include "Opt.h"
#include <stdint.h>

class TOptVendorSpecInfo : public TOpt
{
  public:
    TOptVendorSpecInfo(uint16_t type, char * buf,  int n, TMsg* parent);
    TOptVendorSpecInfo(uint16_t type, uint32_t enterprise, uint16_t sub_option_code,
		       char *data, int dataLen, TMsg* parent);

    size_t getSize();
    char * storeSelf( char* buf);
    bool isValid();
    virtual std::string getPlain();

    uint32_t getVendor();
//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    const std::string& get_Broadband_DeviceManufacturerOUI();
    const std::string& get_Broadband_DeviceSerialNumber();
    const std::string& get_Broadband_DeviceProductClass();
    bool isGet_Broadband_DeviceIdentity();
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD
    ~TOptVendorSpecInfo();
    bool doDuties() { return true; }
protected:
    uint32_t Vendor_;
//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
    std::string Broadband_DeviceManufacturerOUI;
    std::string Broadband_DeviceSerialNumber;
    std::string Broadband_DeviceProductClass;
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD
};

#endif /* OPTVENDORSPECINFO_H */

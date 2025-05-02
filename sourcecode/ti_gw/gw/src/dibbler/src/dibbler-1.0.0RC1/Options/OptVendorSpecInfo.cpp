/*
 * Dibbler - a portable DHCPv6
 *
 * authors: Tomasz Mrugalski <thomson@klub.com.pl>
 *          Marek Senderski <msend@o2.pl>
 *
 * released under GNU GPL v2 licence
 *
 */

#include <string.h>
#include <iostream>
#include <sstream>
#include "Portable.h"
#include "OptVendorSpecInfo.h"
#include "OptGeneric.h"
#include "DHCPConst.h"
#include "Logger.h"

#if defined(LINUX) || defined(BSD)
#include <arpa/inet.h>
#endif

TOptVendorSpecInfo::TOptVendorSpecInfo(uint16_t type, char * buf,  int n, TMsg* parent)
    :TOpt(type, parent), Vendor_(0)
{
    int optionCode = 0, optionLen = 0;
    if (n<4) {
	Log(Error) << "Unable to parse truncated vendor-spec info option." << LogEnd;
        Valid = false;
	return;
    }

    Vendor_ = readUint32(buf); // enterprise number
    buf += sizeof(uint32_t);
    n   -= sizeof(uint32_t);

    while (n>=4) {
        optionCode = readUint16(buf);
        buf += sizeof(uint16_t); n -= sizeof(uint16_t);
        optionLen  =  readUint16(buf);
        buf += sizeof(uint16_t); n -= sizeof(uint16_t);
        if (optionLen>n) {
            Log(Warning) << "Malformed vendor-spec info option. Suboption " << optionCode
                         << " truncated." << LogEnd;
            Valid = false;
            return;
        }

//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
        if (Vendor_ == IANA_ENTERPRISE_BROADBAND_FORUM)
        {
            switch (optionCode)
            {
                case BROADBAND_V6SUBOPT_DEVICE_MANUFACTUREROUI:
                    Broadband_DeviceManufacturerOUI.assign(buf, optionLen);
                    break;
                case BROADBAND_V6SUBOPT_DEVICE_SERIALNUMBER:
                    Broadband_DeviceSerialNumber.assign(buf, optionLen);
                    break;
                case BROADBAND_V6SUBOPT_DEVICE_PRODUCTCLASS:
                    Broadband_DeviceProductClass.assign(buf, optionLen);
                    break;
                default:
                    break;
            }
        }
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD

        SPtr<TOpt> opt = new TOptGeneric(optionCode, buf, optionLen, parent);
        addOption(opt);
        buf += optionLen;
        n   -= optionLen;
    }
    if (n) {
        Log(Warning) << "Extra " << n << " bytes, after parsing suboption " << optionCode
                     << " in vendor-spec info option." << LogEnd;
        Valid = false;
        return;
    }
    Valid = true;
}

TOptVendorSpecInfo::TOptVendorSpecInfo(uint16_t code, uint32_t enterprise,
				       uint16_t sub_option_code,
                                       char *data, int dataLen, TMsg* parent)
    :TOpt(code, parent), Vendor_(enterprise)
{
    if (sub_option_code) {
        SPtr<TOptGeneric> opt = new TOptGeneric(sub_option_code, data, dataLen, parent);
        addOption( (Ptr*) opt);
    }
}

TOptVendorSpecInfo::~TOptVendorSpecInfo() 
{
}

size_t TOptVendorSpecInfo::getSize() {
    SPtr<TOpt> opt;
    unsigned int len = 8; // normal header(4) + enterprise(4)
    firstOption();
    while (opt = getOption()) {
        len += opt->getSize();
    }
    return len;
}

char * TOptVendorSpecInfo::storeSelf( char* buf)
{
    // option-code OPTION_VENDOR_OPTS (2 bytes long)
    buf = writeUint16(buf, OptType);

    // option-len size of total option-data
    buf = writeUint16(buf, getSize()-4);

    // enterprise-number (4 bytes long)
    buf = writeUint32(buf, Vendor_);

    SPtr<TOpt> opt;
    firstOption();
    while (opt = getOption())
    {
        buf = opt->storeSelf(buf);
    }
    
    return buf;
}

std::string TOptVendorSpecInfo::getPlain() {
    std::stringstream tmp;
    tmp << "vendor=" << Vendor_ << " ";

    SPtr<TOpt> opt;
    firstOption();
    while (opt = getOption())
    {
	tmp << opt->getOptType() << "=";
	tmp << opt->getPlain() << " ";
    }
    return tmp.str();
}

bool TOptVendorSpecInfo::isValid()
{
    return true;
}

uint32_t TOptVendorSpecInfo::getVendor()
{
    return Vendor_;
}

//UNIHAN ADD
#ifdef INCLUDE_ARRIS_GW_TR69
const std::string& TOptVendorSpecInfo::get_Broadband_DeviceManufacturerOUI()
{
    return Broadband_DeviceManufacturerOUI;
}

const std::string& TOptVendorSpecInfo::get_Broadband_DeviceSerialNumber()
{
    return Broadband_DeviceSerialNumber;
}

const std::string& TOptVendorSpecInfo::get_Broadband_DeviceProductClass()
{
    return Broadband_DeviceProductClass;
}

bool TOptVendorSpecInfo::isGet_Broadband_DeviceIdentity()
{
    if (Broadband_DeviceSerialNumber.empty()||
        Broadband_DeviceManufacturerOUI.empty())
    {
        /* 
         * Per section 3.8 of ECN 27.1 baseline requirement, 
         * 'The Device MUST support Device-Gateway association 
         * as defined in [TR-069a4] Annex F.'
         * For a DHCP request from the Device that contains the 
         * Device Identity, the DHCP Option MUST contain the 
         * following Encapsulated Vendor-Specific Option-Data fields:
         * DeviceManufacturerOUI
         * DeviceSerialNumber
         * DeviceProductClass (this MAY be left out if the corresponding
         * source Parameter is not present)
         */
        return false;
    }

    return true;
}
#endif //INCLUDE_ARRIS_GW_TR69
//END UNIHAN ADD


/*  Copyright 2008-2009, ARRIS Group, Inc., All rights reserved                              */
/*
////////////////////////////////////////////////////////////////////////////////
//
// arris_customers.h     Definitions for the Arris Customers
//
////////////////////////////////////////////////////////////////////////////////
*/

#ifndef __ARRIS_CUSTOMERS_H__
#define __ARRIS_CUSTOMERS_H__

/*
*  Below enum for Customer ID should be in sync with the enum mentioned 
*  in Dev Wiki Page : http://devwiki.arrisi.com/wiki/IPTV_DEV_BRCM#Customer_Enumeration
*  Make sure to update the enum in Dev Wiki Page when the below enum is
*  changed.
*/

//Customer index. This is used with MFG_CUST_INDEX NVM setting
enum
{
    CUSTOMER_RESERVED,      // 0
    CUSTOMER_DEFAULT,       // 1
    CUSTOMER_COMCAST,       // 2
    CUSTOMER_TIMEWARNER,    // 3
    CUSTOMER_CABLEVISION,   // 4
    CUSTOMER_COX,           // 5
    CUSTOMER_VTR,           // 6
    CUSTOMER_UPC,           // 7
    CUSTOMER_VIRGIN,        // 8
    CUSTOMER_TELENET,       // 9
    CUSTOMER_JNC,           // 10
    CUSTOMER_CHARTER,       // 11
    CUSTOMER_SUDDENLINK,    // 12
    CUSTOMER_BEND,          // 13
    CUSTOMER_BUCKEYE,       // 14
    CUSTOMER_COMPORIUM,     // 15
    CUSTOMER_NEWWAVE,       // 16
    CUSTOMER_ROGERS,        // 17
    CUSTOMER_SHAW,          // 18
    CUSTOMER_WAVE,          // 19
    CUSTOMER_ZIGGO,         // 20
    CUSTOMER_OPEN_21,       // 21
    CUSTOMER_COGECO,        // 22
    CUSTOMER_MAX            // 23
};

#endif /* __ARRIS_CUSTOMERS_H__ */

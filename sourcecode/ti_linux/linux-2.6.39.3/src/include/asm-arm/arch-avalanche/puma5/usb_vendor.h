#ifndef _USB_VENDOR_H
#define _USB_VENDOR_H

// ARRIS has customized this file for our modems!!

/* vendor specific configuration definitons
   for usb cdc/rndis gadget driver */
#define DEFAULT_USB_VENDOR_ID           0x09C1 /* ARRIS */
#define DEFAULT_USB_PRODUCT_PUMA3       0x6104 /* TI-CM Puma3*/
#define DEFAULT_USB_PRODUCT_PUMAS       0x8050 /* TI-CM PumaS*/
#define DEFAULT_USB_PRODUCT_PUMA5       0x1337 /* Puma5*/
#define DEFAULT_USB_PRODUCT_ID          (DEFAULT_USB_PRODUCT_PUMA5)
#define DEFAULT_USB_VENDOR_NAME         "ARRIS"
#define DEFAULT_USB_VENDOR_DESC         "ARRIS RNDIS Adapter"
#define DEFAULT_USB_INSTANCE            "000001"
#define DEFAULT_USB_CONFIG              "USB RNDIS Configuartion"
#define DEFAULT_USB_COMM_IF             "Communication Interface"
#define DEFAULT_USB_DATA_IF             "Data Interface"
#define DEFAULT_USB_PC_MAC_ADDRESS      "00:00:CA:00:00:04"
#define DEFAULT_USB_DEVICE_MAC_ADDRESS  "00:00:CA:00:00:03"

#endif 

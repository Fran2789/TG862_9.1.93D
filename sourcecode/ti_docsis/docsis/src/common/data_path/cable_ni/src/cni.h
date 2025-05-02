/*  Copyright 2011, ARRIS Group, Inc., All rights reserved  */
  
typedef struct
{
    unsigned int    cmd;    /**< Command */
    void            *data;  /**< Data provided with the command - depending upon command */
} CniDrvPrivIoctl;

typedef enum
{
    DISABLE_FILTER,
    ENABLE_FILTER_WITH_BYPASS,
    ENABLE_FILTER_WITHOUT_BYPASS
} CniSocketFilter_t;

typedef struct
{
    CniSocketFilter_t  enable;
    struct sockaddr addr;
} CniDrvDbridgeBypass;

typedef struct
{
    unsigned long       rx_packets;         /* total packets received	 */
    unsigned long       tx_packets;         /* total packets transmitted */
    unsigned long long  rx_bytes;           /* total bytes received 	 */
    unsigned long long  tx_bytes;           /* total bytes transmitted	 */
    unsigned long       rx_errors;          /* bad packets received		 */
    unsigned long       tx_errors;          /* packet transmit problems	 */
    unsigned long       rx_drops;           /* dropped receive packets   */
    unsigned long       tx_drops;           /* dropped tranmit packets   */

    unsigned long       filtered_rx_packets;/* matched rx packets */
    unsigned long       filtered_rx_bytes;  /* matched rx bytes   */
    unsigned long       filtered_tx_packets;/* matched tx packets */
    unsigned long       filtered_tx_bytes;  /* matched tx bytes   */

    int lro_enabled;
    unsigned long       rx_aggregated;      /* total packets processed by LRO */
    unsigned long       rx_flushed;         /* total handoffs to IP from LRO  */
} CniDrvStats;

/* CNI new Ioctl's created - using a value with a base that can be adjusted as per needs */
#define CNI_IOCTL_BASE          0

/* Filtering IOCTL */
#define CNI_LRO_ENABLE          (CNI_IOCTL_BASE + 0)
#define CNI_DOCBRIDGE_BYPASS    (CNI_IOCTL_BASE + 1)
#define CNI_GET_STATS           (CNI_IOCTL_BASE + 2)

/*
*  Copyright 2008, Texas Instruments Incorporated.
*
* Description:
*   This interface is the bridge between user and kernel space for the TIUHAL
*   module.
*
*/

#ifndef  TIUHAL_LIN_FILE_HEADER_INC
#define  TIUHAL_LIN_FILE_HEADER_INC

/* The DEVICE_NAME is used by the kernel module and the DEVICE_PATH is used by
 * the userspace application.  BOTH MUST have the SAME basename in order for
 * this to work properly.
 */
#define TIUHAL_DEVICE_NAME "titele"
#define TIUHAL_DEVICE_PATH "/dev/titele"


/*******************************************************************************
 *  These definitions are used to communicate with the TIUHAL module
 ******************************************************************************/
#define TIUHAL_LIN_IOCTL_MAGIC 'N'
#define TIUHAL_LIN_IOCTL_MREAD  _IOWR(TIUHAL_LIN_IOCTL_MAGIC, 0,TIUHAL_LIN_RW_IOCTL_T)
#define TIUHAL_LIN_IOCTL_MWRITE _IOWR(TIUHAL_LIN_IOCTL_MAGIC, 1,TIUHAL_LIN_RW_IOCTL_T)
#define TIUHAL_LIN_IOCTL_CS_REG _IOW(TIUHAL_LIN_IOCTL_MAGIC,  2,TIUHAL_LIN_CS_IOCTL_T)
#define TIUHAL_LIN_IOCTL_IOCTL  _IOWR(TIUHAL_LIN_IOCTL_MAGIC, 3,TIUHAL_LIN_IOCTL_IOCTL_T)
#define TIUHAL_LIN_IOCTL_CONFIG _IOR(TIUHAL_LIN_IOCTL_MAGIC,  4,TIUHAL_LIN_CONFIG_IOCTL_T)

#define TIUHAL_LIN_IOCTL_MAXNR 4 

#define TIUHAL_LIN_MAX_RD_WR_BYTES    80
#define TIUHAL_IOCTL_MAX_IOCTL_BYTES  32

#ifdef LANTIQ
#define TIUHAL_FLAGS_TID_DEFAULT 0
#define TIUHAL_FLAGS_TID_LANTIQ  1
#endif /* LANTIQ */

/*******************************************************************************
 *  The types and structures used in common between user and common code.
 ******************************************************************************/
typedef struct 
{
	UINT32			tcid; /* Keeping it to UINT32 to avoid alignment issues */
#ifdef LANTIQ
    UINT32                  flags;
#endif /* LANTIQ */

	TIUHAL_DATA_T 	*bytestream;
	TIUHAL_REG_T  	reg;
	UINT32        	data_len;
	TIUHAL_IO_ERR_T return_value;
}TIUHAL_LIN_RW_IOCTL_T;

typedef struct
{
	UINT32			tcid; /* Keeping it to UINT32 to avoid alignment issues */
	TIUHW_CHIPSEL_T chipselect_data;
}TIUHAL_LIN_CS_IOCTL_T;

typedef struct
{
	UINT32 return_code;
	UINT32 cmd;
	void   *in_param;
	void   *out_param;
	UINT32 num_bytes_in;
	UINT32 num_bytes_out;	
}TIUHAL_LIN_IOCTL_IOCTL_T;

typedef struct
{
	BOOL   interrupts_enabled;
	UINT32 version;
}TIUHAL_LIN_CONFIG_IOCTL_T;

/*******************************************************************************
 *  Internal HAL function prototypes.
 ******************************************************************************/
int  tiuhal_board_init(TIUHAL_CAP_T *capabilities);
void tiuhal_board_teardown(void);


#endif   /* ----- #ifndef TIUHAL_LIN_FILE_HEADER_INC  ----- */


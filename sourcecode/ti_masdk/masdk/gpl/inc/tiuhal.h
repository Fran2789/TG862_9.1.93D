/*
*  Copyright 2003-2008 by Texas Instruments Incorporated.
*
* Description:
*  This include file contains API and structure defintions for the 
*  Voice/telephony Hardwae Abstraction Layer (TIUHAL) module 
*  (not to be confused with the AIC module found in some SOC's).
*
*  This is a platform neutral file that has several configurable
*  options.  Please refer to your release's documentation for 
*  details of what is required to be supported.
*
*/

#ifndef __TIUHAL_H__
#define __TIUHAL_H__


/*******************************************************************************
 * Nested Includes
 ******************************************************************************/

#include "tiuhalcfg.h" /* Defines needed to turn on certain hal capabilities */

/** @addtogroup TELEPHONY_HAL 
 * @brief
 *
 * This module defines the function interfaces and other prototypes found
 * typically in a Telephony Hardwre Abstration Layer (TIUHAL) implementation.  
 * Some API's are marked optional - depending on the specific hardware 
 * implmentation and requirements.  These optional API's may be conditionally
 * compiled out without loss of execution in some cases.  Please refer to your 
 * TIUHAL and TIUHW implementation documentation for what functionality is 
 * implemented and/or required.
 */
/**@{*/

/*******************************************************************************
 * Typedefs
 ******************************************************************************/

/** 
 * @brief This typedef is used by the TIUHAL to steer any platform specifc
 * code to the correct CS on a system.
 * Enabled if TIUHW_CMN_CS_TYPE is defined.
 */

#ifdef TIUHW_CMN_CS_TYPE
typedef UINT32 TIUHW_CHIPSEL_T;
#endif 

#ifdef TIUHAL_USE_CMN_CAP_T
/** 
 * @brief This information is gathered after init is called, if set to NULL then
 * this info will not be filled in during init. This particular implementation is
 * enabled if TIUHAL_USE_CMN_CAP_T is defined.
 */

typedef struct 
{
    UINT32    version;        /* For FPGA based systems, this may be the version + route type */
}TIUHAL_CAP_T;
#endif

#ifdef TIUHAL_USE_CMN_INIT_ERR_T
/** 
 * @brief This enum contains common error codes used during TIUHAL initialization.
 * This particular implementation is defined if TIUHAL_USE_CMN_INIT_ERR_T is set.
 *
 * TIUHAL_INIT_OK - Initialization succesful.
 *
 * TIUHAL_INIT_RESOURCE - resource error (memory/device/etc.)
 *
 * TIUHAL_INIT_GEN_ERROR - other error
 *
 * TIUHAL_INIT_CFG_ERROR - configuration/options error
 *
 * @sa TIUHAL_API_T:init
 */
typedef enum
{
    TIUHAL_INIT_OK,
    TIUHAL_INIT_RESOURCE, 
	TIUHAL_INIT_GEN_ERROR,
	TIUHAL_INIT_CFG_ERROR,
    TIUHAL_INIT_LAST
} TIUHAL_INIT_ERROR_T;
#endif /* TIUHAL_USE_CMN_INIT_ERR_T */

#ifdef TIUHAL_USE_CMN_IO_ERROR_T
/** 
 * @brief This enum contains common error codes used during read/write and other
 * funcitons.  This particular implementation is used if 
 * TIUHAL_USE_CMN_IO_ERROR_T is defined.
 * 
 * TIUHAL_IO_OK - operation succeeded.
 *
 * TIUHAL_IO_TIMEOUT - operation timedout
 *
 * TIUHAL_IO_GEN_ERROR - unspecified error condition
 *
 * TIUHAL_IO_INVALID_TCID - channel to be operated on isn't a valid h/w TCID
 *
 * @sa TIUHAL_API_T:read, TIUHAL_API_T:write, TIUHAL_API_T:ringer 
 */

typedef enum 
{
    TIUHAL_IO_OK,
    TIUHAL_IO_TIMEOUT,
    TIUHAL_IO_GEN_ERROR,
    TIUHAL_IO_INVALID_TCID,
    TIUHAL_IO_LAST
} TIUHAL_IO_ERR_T;
#endif /* TIUHAL_USE_CMN_IO_ERROR_T */

/*******************************************************************************
 * TIUHAL API - use tiudrv.h to turn on certain API's.
 ******************************************************************************/
#ifdef TIUHAL_USE_CMN_REG_T 
typedef UINT8 TIUHAL_REG_T;
#endif

#ifdef TIUHAL_USE_CMN_DATA_T
typedef UINT8 TIUHAL_DATA_T;
#endif
/** 
 * @brief This data structure contains the function pointer interface table
 * used by the TIUHW driver layer to communicate with the various telephony
 * hardware chipsets.
 */
typedef struct 
{

/**
 * @brief 
 * 	This function is called to initialize the Telephony Hardware Abstraction
 * 	layer (TIUHAL).
 *
 * @param[in,out]  TIUHAL_CAP_T * - the capabilities pointer.  Can be set to
 * NULL to not fill this in.
 * @param[in] void * options - if used by the HAL, contains dynamic options
 * that are used during initialization.
 * @retval TIUHAL_INIT_ERROR_T - TIUHAL_INIT_OK if successful.
 *
 *
 * @pre No assumptions are made.
 * @post The needed resources and hardware are initialized
 *
 * @code 
 * TIUHAL_INIT_ERROR_T  (*init)(TIUHAL_CAP_T *capabilities, void *options); 
 * @endcode 
 *
 * @remarks 
 *
 *    This function is platform specific since it may call out for a
 *    particular platforms routines during initialization.  
 *
 *    For a SPI interface, this function may initialize the SPI clocks, 
 *    the SPI chipselects and configure for the particular word mode 
 *    supported by the platform.
 *
 *    For a T1/E1 inteface, the devices may be taken out of reset so that
 *    a master clock can be recovered.
 *
 * @sa TIUHAL_API_T::teardown
 */

    TIUHAL_INIT_ERROR_T  (*init)(TIUHAL_CAP_T *capabilities, void *options);    

/**
 * @brief 
 * 	This function reads data from a particular telephony chipset register and 
 * 	returns it back to the caller.
 *
 * @param[in] TCID   tcid                 - the hardware channel to read from
 * @param[in] TIUHAL_REG_T   reg          - register address
 * @param[in] TIUHAL_DATA_T *bytestream   - where to place the data (assumes caller
 *                                          allocated buffer)
 * @param[in] UINT32 length               - the number of TIUHAL_DATA_T to read
 * @retval    TIUHAL_IO_ERR_T             - transfer result, TIUHAL_IO_OK for 
 *                                          success.
 *
 * @pre  init() was called and the TID is out of reset.
 * @post the data is read from the device channel or error is generated.
 *
 * @code 
 * TIUHAL_IO_ERR_T (*read)(TCID tcid, 
 * 		TIUHAL_REG_T reg, 
 * 		TIUHAL_DATA_T *bytestream, 
 * 		UINT32 length); 
 * @endcode 
 *
 * @remarks 
 *    This function is platform specific since it may call out for a
 *    particular platforms routines during operation (chipselect logic, etc.).  The
 *    actual implementation will vary from platform to platform, but should 
 *    typically deassert chipselect between byte transfers.  
 *
 *    NOTE:
 *    Normally, a read operation occurs prior to a write to indicate to the 
 *    chipset which register to access.
 *
 *    In some cases the implementation will be a serial port (such as SPI) 
 *    and in others a parallel interface.  Please refer to your platform's 
 *    documentation on specific implementation.  
 *
 * @sa TIUHAL_API_T::write, TIUHAL_API_T:init
 */
    TIUHAL_IO_ERR_T (*read)(TCID tcid, TIUHAL_REG_T reg, TIUHAL_DATA_T *bytestream, UINT32 length); 

/**
 * @brief 
 * 	This function sends the control data to a specific telephony chipset.
 *
 * @param[in] TCID   tcid                 - the hardware channel to receive the 
 *                                          bytestream
 * @param[in] TIUHAL_REG_T   reg          - register address
 * @param[in] TIUHAL_DATA_T *bytestream   - the data to send 
 * @param[in] UINT32 length               - the number of TIUHAL_DATA_T to transfer
 * @retval    TIUHAL_IO_ERR_T             - transfer result, TIUHAL_IO_OK for 
 *                                          success.
 *
 * @pre  init() was called and the TID is out of reset.
 * @post the data is sent to the device channel or error is generated.
 *
 * @code 
 * TIUHAL_IO_ERR_T (*write)(TCID tcid, 
 * 		TIUHAL_REG_T reg, 
 * 		TIUHAL_DATA_T *bytestream, 
 * 		UINT32 length); 
 * @endcode 
 *
 * @remarks 
 *    This function is platform specific since it may call out for a
 *    particular platforms routines during operation (chipselect logic, etc.).  The
 *    actual implementation will vary from platform to platform, but should 
 *    typically deassert chipselect between byte transfers.  
 *
 *    In some cases the implementation will be a serial port (such as SPI) 
 *    and in others a parallel interface.  Please refer to your platform's 
 *    documentation on specific implementation.  
 *
 * @sa TIUHAL_API_T:write, TIUHAL_API_T:init
 */

    TIUHAL_IO_ERR_T (*write)(TCID tcid, TIUHAL_REG_T reg, TIUHAL_DATA_T *bytestream, UINT32 length);
/**
 * @brief 
 * 	This function implements a generic IOCTL API.  
 *
 * @param[in] TCID          tcid          - the hardware channel to receive the 
 *                                          bytestream
 * @param[in] UINT32        cmd           - the IOCTL to perform.
 * @param[in,out] void       *in           - Typically the input stream pointer
 * @param[in,out] void       *out          - Typically the output stream pointer
 * @retval       UINT32                   - result code (if any)
 *
 * @pre  init() was called
 * @post if the particular IOCTL cmd is supported, it will be acted upon.
 *
 * @code 
 *  UINT32          (*ioctl)(TCID tcid, UINT32 cmd, void *in, void *out);
 * @endcode 
 *
 * @remarks 
 *    This function is platform specific since it may call out for a
 *    particular platforms routines during operation (chipselect logic, etc.).  The
 *    actual implementation will vary from platform to platform, but should 
 *    typically deassert chipselect between byte transfers (if any) 
 *
 *    In some cases the implementation will be a serial port (such as SPI) 
 *    and in others a parallel interface.  Please refer to your platform's 
 *    documentation on specific implementation.  
 *
 * @sa TIUHAL_API_T:init, TELEPHONY_HAL_IOCTL_DEFINES
 */

	UINT32	(*ioctl)(TCID tcid, UINT32 cmd, void *in, void *out);

#ifdef TIUHAL_HAS_TID_INTS
/**
 * @brief 
 * 	This function implements masking of interrupts on systems that rely upon TIUHAL
 * 	to perform this function, typically a FPGA or PLD based system.  This is an 
 * 	OPTIONAL API (if TIUHAL_HAS_TID_INTS defined).
 *
 * @param[in]    UINT8      interrupt     - which interrupt to mask.
 * @param[in]    BOOL       mask          - if TRUE will mask the particular 
 *                                          interrupt.
 * @retval       void
 *
 * @pre  init() was called and the underlying hardware is initialized (FPGA 
 *       loaded, PLD programmed, etc.)
 * @post The interrupt is masked.
 *
 * @code 
 *  void    (*mask_interrupt)(UINT8 interrupt, BOOL mask);  
 * @endcode 
 *
 * @remarks 
 *
 * This is a OPTIONAL API that is implemented if TIUHAL_HAS_TID_INTS is defined. 
 * The meaning of the interrupt is up to the platform implementation to define.
 *
 * @sa TIUHAL_API_T:init, TIUHAL_API_T:clear_interrupt, TIUHAL_API_T:get_interrupt_status
 */

    void    (*mask_interrupt)(UINT8 interrupt, BOOL mask);              
/**
 * @brief 
 * 	This function implements clearing of interrupts on systems that rely upon TIUHAL
 * 	to perform this function, typically a FPGA or PLD based system.  This is an 
 * 	OPTIONAL API (if TIUHAL_HAS_TID_INTS is defined).
 *
 * @param[in]    UINT8      interrupt     - which interrupt to clear
 * @retval       void
 *
 * @pre  init() was called and the underlying hardware is initialized (FPGA 
 *       loaded, PLD programmed, etc.)
 * @post The interrupt is clear
 *
 * @code 
 *  void    (*clear_interrupt)(UINT8 interrupt)
 * @endcode 
 *
 * @remarks 
 *
 * This is a OPTIONAL API that is implemented if TIUHAL_HAS_TID_INTS is defined. 
 * The meaning of the interrupt is up to the platform implementation to define.
 *
 * @sa TIUHAL_API_T:init, TIUHAL_API_T:mask_interrupt, TIUHAL_API_T:get_interrupt_status
 */

    void    (*clear_interrupt)(UINT8 interrupt);

/**
 * @brief 
 * 	This function retrieves the interrupt status from a FPGA or PLD based system.
 * 	This is an OPTIONAL API (if TIUHAL_HAS_TID_INTS is defined).
 *
 * @retval UINT32 - the interrupt status.
 *
 * @pre  init() was called and the underlying hardware is initialized (FPGA 
 *       loaded, PLD programmed, etc.)
 * @post The interrupt status is returned.
 *
 * @code 
 *  UINT32 (*get_interrupt_status)(void);
 * @endcode 
 *
 * @remarks 
 *
 * This is a OPTIONAL API that is implemented if TIUHAL_HAS_TID_INTS is defined. 
 * The meaning of the interrupt status is up to the platform implementation to 
 * define.
 *
 * @sa TIUHAL_API_T:init, TIUHAL_API_T:clear_interrupt, TIUHAL_API_T:mask_interrupt
 */

    UINT32  (*get_interrupt_status)(void);
#endif 

#ifdef TIUHAL_HAS_DETECT
/**
 * @brief 
 * 	This function retrieves the ring detection of a telephony chipset.
 * 	This is an OPTIONAL API (if TIUHAL_HAS_DETECT is defined)
 *
 * @retval UINT32 - the ring detection state.
 *
 * @pre  init() was called and the underlying hardware is initialized (FPGA 
 *       loaded, PLD programmed, etc.)
 * @post The ring detection is returned.
 *
 * @code 
 *  UINT32 (*get_detect_status)(void);
 * @endcode 
 *
 * @remarks 
 *
 * This OPTIONAL API is used on chipsets that report ring detection via a 
 * pin and the platform supports this.
 *
 * @sa TIUHAL_API_T:init
 */

    UINT32  (*get_detect_status)(void);                                 
#endif

#ifdef TIUHAL_HAS_RINGER                                                /* enable if the HAL is to generate a ring signal toward the TID */
/**
 * @brief 
 * 	This OPTIONAL function is used to generate a ring signal toward a chipset. 
 * 	It is enabled if TIUHAL_HAS_RINGER is defined.
 *
 * @param[in] TCID tcid - the channel to generate the ring.
 * @param[in] BOOL ringer_on - if the ringer is on (TRUE), or off (FALSE)
 * @retval TIUHAL_IO_ERR_T - did the operation succeed. 
 *
 * @pre  init() was called and the underlying hardware is initialized (SOC,
 *       FPGA, or PLD)
 * @post The ring state is what is specified or error.
 *
 * @code 
 * TIUHAL_IO_ERR_T (*ringer)(TCID tcid, BOOL ringer_on);
 * @endcode 
 *
 * @remarks 
 *
 * This OPTIONAL API is used on chipsets that can't generate the ringer
 * shape needed on its own (typically 20Hz sinusoid in the US).  The 
 * actual shape generated is not defined here, but may be passed either 
 * through a custom IOCTL or #defined in the code.
 *
 * @sa TIUHAL_API_T:init, TIUHAL_IOCTL_CONFIG_RINGER
 */

    TIUHAL_IO_ERR_T (*ringer)(TCID tcid, BOOL ringer_on);
#endif

#ifdef TIUHAL_HAS_TEARDOWN
/**
 * @brief 
 * 	This OPTIONAL function is used to deallocate any resources allocated 
 *  during initialization. It is required if TIUHAL_HAS_TEARDOWN is defined.
 *
 * @param[in] void *options - any options needed by the teardown function.
 * @retval BOOL - TRUE if successful.
 *
 * @pre  init() was called and the options contain any meaningful data for
 *       deallocation.
 * @post the return code of TRUE indicates successful deallocation.
 *
 * @code 
 * BOOL    (*teardown) (void *options);
 * @endcode 
 *
 * @remarks 
 *
 * This OPTIONAL API is used for deallocating resources, such as device files
 * or memory on systems that allow for unloading and reloading of the TIUHAL.
 *
 * @sa TIUHAL_API_T:init
 */

    BOOL    (*teardown) (void *options);                                
#endif
} TIUHAL_API_T;

/** @addtogroup TELEPHONY_HAL_IOCTL_DEFINES
 * @brief
 * The TIUHALs may support several different IOCTL's. This section defines some
 * of the common ones anticipated. If not supported, the IOCTL is ignored.
 * A "Safe" return value of 0 and NULL for output is typically returned in this 
 * situation.  ALL these IOCTLS's are optional and are not required to be
 * implemented by the developer (unless their specific h/w requires it!).
 *
 ******************************************************************************/
/** @{ */

/**  @def  TIUHAL_IOCTL_GET_TID_TYPE 
 * Used to get TID ID's in FPGA/PLD based systems, out pointer contains TID_ID 
 */
#define TIUHAL_IOCTL_GET_TID_TYPE   0x0001    

/** @def TIUHAL_IOCTL_SET_IF_MODE
 *  Used to select byte mode in FPGA's, not supported by all HAL's, 
 * sometimes used to change byte mode 
 */
#define TIUHAL_IOCTL_SET_IF_MODE    0x0002    

/** @def TIUHAL_IOCTL_SET_ROUTE
 * Used for setting the route of a FPGA, in  would contain route for the 
 * particular channel 
 */
#define TIUHAL_IOCTL_SET_ROUTE      0x0003    

/** @def TIUHAL_IOCTL_SET_LOOPBACK
 *  Used for FPGA's that can set PCM loopback
 */
#define TIUHAL_IOCTL_SET_LOOPBACK   0x0004    

/** @def TIUHAL_IOCTL_SET_TID_TYPE
 * Used for FPGA's that need to set the TID type, 
 * in ptr would contain the TCID_DRVR
 */
#define TIUHAL_IOCTL_SET_TID_TYPE   0x0005  
                                          
/** @def TIUHAL_IOCTL_RESET_TID
 *  Used for FPGA/PLD systems that are used to reset the TIDs
 */
#define TIUHAL_IOCTL_RESET_TID      0x0006    

/** @def TIUHAL_IOCTL_CONFIG_RINGER
 * Used to configure the SOC/FPGA/PLD ringer parameters.  
 * @sa TIUHAL_API_T::ringer
 */
#define TIUHAL_IOCTL_CONFIG_RINGER  0x0007    

/** @def TIUHAL_IOCTL_GET_SCRATCHPAD
 * Used to get any scratch pad memory (mainly for FPGA's), 
 * NULL for out is set if this capability is NOT available
 */
#define TIUHAL_IOCTL_GET_SCRATCHPAD 0x0008   
                                           

/** @} */

/** @addtogroup TELEPHONY_HAL_PLATFORM_FUNCTIONS
 * @brief This section contans function macros expected to be implemented in
 * the platform layer.  Typically this is to select and deselect the particular
 * hardware channel of a device.
 */
/** @{ */

#ifndef TIUHAL_SELECT_TID
/**
 * @def TIUHAL_SELECT_TID(TCID)
 * @param[in] TCID - which channel is the TIUHAL trying to access.  This code
 * is provided by the platform implementator to drive things like GPIOs or
 * other steering logic.
 */
#define TIUHAL_SELECT_TID(TCID_id) tiuhw_select_tid(TCID_id)
int tiuhw_select_tid(TCID id);
#endif

#ifndef TIUHAL_DESELECT_TID
/**
 * @def TIUHAL_DESELECT_TID(TCID)
 * @param[in] TCID - which channel is the TIUHAL trying to deselect. This code
 * is provided by the platform implementator to drive things like GPIOs or
 * other steering logic.
 */
#define TIUHAL_DESELECT_TID(TCID_id) tiuhw_deselect_tid(TCID_id)
void tiuhw_deselect_tid(TCID id);
#endif

/**@}*/  

/*******************************************************************************
 * Variables
 ******************************************************************************/

/**
 * @brief This is the pointer to be used by ALL TID drivers to hit the 
 * read/write/HAL functionality. In some cases the TID driver supplier may
 * have a specified API that does not conform to TI's.  In this case an adaption
 * layer is required.
 */
extern TIUHAL_API_T *tiuhw_api; 
/**@}*/  /* end addtogroup TELEPHONY*/

#endif

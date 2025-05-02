/*
 * bbu_boot.c
 * Description:
 * See below.
 *
 *
 * Copyright (C) 2008, Texas Instruments, Incorporated
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#define _BBU_BOOT_C_

/*! \file bbu_boot.c
    \brief BBU rouitines for enabling batteries in bootloader
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/

#include <puma5.h>

/* printf*/
#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <zlib.h>
#include <bzlib.h>
#include <environment.h>
#include <asm/byteorder.h>

#include "bbu_boot_params.h"

/**************************************************************************/
/*      EXTERNS Declaration:                                              */
/**************************************************************************/
extern int psc_enable_module (int module_id);

/**************************************************************************/
/*      DEFINES:                                                          */
/**************************************************************************/

/*! \def BBU_HAL_DEBUG
 *  \brief macro for controlling debug prints  
 */
#define BBU_HAL_DEBUG (printEnabled == 1)

/* battery specific code  - subset of the one in bbu_bat_cfg.c */

#define DESC(batid, maxVolt, minvolt, maxTempCharge, minTempCharge, maxTempDischarge, minTempDischarge, maxCurrCharge, maxCurrDischarge,  bbuIdLineVal) \
        {bbuIdLineVal, INIT_MAXVOLT(maxVolt), INIT_MINVOLT(minvolt), INIT_MAXTEMPCHARGE(maxTempCharge), INIT_MINTEMPCHARGE(minTempCharge), INIT_MAXTEMPDISCHARGE(maxTempDischarge), \
        INIT_MINTEMPDISCHARGE(minTempDischarge), INIT_CURRENT(maxCurrCharge), INIT_CURRENT(maxCurrDischarge)},

static const Bbu_BatterySpecificDataBoot_t Bbu_BatterySpecificData_array[] = 
{
    BAT_TABLE
};

#undef DESC

#define CURRENT_DISCHARGE (1)
#define CURRENT_NOT_DISCHARGE (0)

/**************************************************************************/
/*      LOCAL DECLARATIONS:                                               */
/**************************************************************************/

static short bbuBootCheckIfBatteryPresent(ONE_SHOT_DATA_TYPE *pBbuData, int *direction, unsigned short *batId, int *batTableIndex);
static int bbuBootCheckSafety(ONE_SHOT_DATA_TYPE *pBbuData, int batTableIndex, int direction);
static unsigned long bbuBootGetRegValue(unsigned long regAddr);
static void bbuBootSetRegValue(unsigned long regAddr, unsigned long val);
static int bbuBootAdcSetToIdle(void);
static int bbuBootSetUpSingleSample(int channel);
static int bbuBootSetUpSingleSampleAndPoll(int channel, unsigned short *pData);
static int bbuBootSetUpOneShotSample(void);
static int bbuBootOneShotSamplePoll(ONE_SHOT_DATA_TYPE *pData);
static int bbuBootSetUpOneShotSampleAndPoll(ONE_SHOT_DATA_TYPE *pData);
static int bbuBootSetAveragingMode(BBU_AVERAGING_MODE_TYPE mode);
static int bbuBootSetBbuClockDivider(unsigned short divider);
static int bbuBootEnableBattery(unsigned short index, unsigned short enable);
static unsigned short bbuBootAveragingCompensation(unsigned short value, int dir);
static unsigned short bbuBootConvertHwToPure_Single(unsigned short hwValue);
static int bbuBootIsBbuSupported(void);
static int bbuBootConvertHwToPure_Diff(unsigned short hwValue);
static int bbuIsPowerReset(void);

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/

static int printEnabled = 0;

/**************************************************************************/
/*      INTERFACE FUNCTIONS Implementation:                               */
/**************************************************************************/

/**************************************************************************/
/*                  bbu_init()                                            */
/**************************************************************************/
/* DESCRIPTION: initializes the BBU system and enables battery            */
/*                       if one found                                     */
/* INPUT:       printEnableParam -1 if debug print requested              */
/*                                                                        */
/* OUTPUT:      none                                                      */
/*                                                                        */
/**************************************************************************/
void bbu_init (int printEnableParam)
{
    unsigned long regVal;
    unsigned short dummy;
    ONE_SHOT_DATA_TYPE bbuData;
    short batteryEnabledIndex = 0;
    unsigned short safetyOk = 1;
    int i;
    int currentDirection;
    unsigned short batId;
    int batTableIndex = -1;

    printEnabled = printEnableParam;
    /**********************************/
    /* Check if platform supports BBU */
    /**********************************/
    if (bbuBootIsBbuSupported() == 0)
    {
        return;
    }

    /******************************************************************/
    /* check if this is a power reset. if it's not- dont do anything, */
    /* the existing situation is as software left it before           */
    /******************************************************************/

    if (!(bbuIsPowerReset()))
    {
        if (BBU_HAL_DEBUG)
        {
            printf("%s: Not power reset, return \n", __FUNCTION__);
        }
        return;
    }

    /* Taking the BBU module out of reset in PRCR */
    if (BBU_HAL_DEBUG)
        printf("%s: Take BBU module out of reset \n", __FUNCTION__);

    psc_enable_module(LPSC_BBU);

    /* set initial clock rate to maximal rate of ~2MHz (25MHz/13) */
    bbuBootSetBbuClockDivider(PCLKCR1_PUMA5_BBU_CLK_DIV_MIN);

    /* Set ADC to Idle state. */
    bbuBootAdcSetToIdle();

    /* set averaging mode to the default - x16 */
    bbuBootSetAveragingMode(AVERAGING_MODE_16);

    /* Analog Control Register settings */
    regVal = bbuBootGetRegValue(BBU_ACTRL_PUMA5); /* read ACTRL register value */
    regVal |= BBU_ACTRL_PUMA5_DIFF_MASK;  /* set 'DIFF' to 1 */ 

    regVal &= ~BBU_ACTRL_PUMA5_GAINSEL_MASK; /* zero gain bits */
    regVal |= (BBU_DEFAULT_GAIN_FOR_BL<<BBU_ACTRL_PUMA5_GAINSEL_SHIFT); /* set gain */

    bbuBootSetRegValue(BBU_ACTRL_PUMA5, regVal); /* write register */

    /* PWM does not need to be enabled in the bootloader */
    if (BBU_HAL_DEBUG)
    {
        printf("%s: get dummy sample\n", __FUNCTION__);
    }

    for (i=0; i<2; i++)
    {
        /* Generate 2 "Dummy" samples - workaround for ADC first-sample hardware issue. */
        if (bbuBootSetUpSingleSampleAndPoll(0, &dummy) != 0)
        {
            if (BBU_HAL_DEBUG)
            {
                printf("\n bbuBootSetUpSingleSampleAndPoll failed\n");
            }
            return;
        }
    }
    if (BBU_HAL_DEBUG)
    {
        printf("%s: get a real  sample\n", __FUNCTION__);
    }

    /* now do a shot sample*/
    if (bbuBootSetUpOneShotSampleAndPoll(&bbuData) == -1)
    {
        if (BBU_HAL_DEBUG)
        {
            printf("%s: bbuBootSetUpOneShotSampleAndPoll fail, can't check batteries \n", __FUNCTION__);
        }

        return;
    }

    /* need to analyze the results. Check validity, check if running from battery and which one*/
    batteryEnabledIndex = bbuBootCheckIfBatteryPresent(&bbuData, &currentDirection, &batId, &batTableIndex);

    /* check security thresholds*/
    /* if succeed  enable battery */
    if (batteryEnabledIndex != -1)
    {
        if (batTableIndex != -1)
        {
            safetyOk = bbuBootCheckSafety(&bbuData, batTableIndex, currentDirection);
        }
        else
        {
            if (BBU_HAL_DEBUG)
            {
                printf("\n battery %d found but no id in safety table (%d). Assume safety ok, enable battery  \n", batId);
            }
        }

        if (safetyOk)
        {
            if (BBU_HAL_DEBUG)
            {
                printf("\n safety check ok, enable battery  \n");
            }

            bbuBootEnableBattery(batteryEnabledIndex, 1 /* enable*/);
            bbuBootEnableBattery((batteryEnabledIndex == BBU_EN_BAT1_INDEX) ? BBU_EN_BAT2_INDEX : BBU_EN_BAT1_INDEX, 0 /* disable*/);
        }
    }
    else
    {
        if (BBU_HAL_DEBUG)
            printf("\n no battery enabled \n");
    }

    return;
}

/**************************************************************************/
/*      LOCAL FUNCTIONS:                                                  */
/**************************************************************************/

/**************************************************************************/
/*                  bbuBootGetRegValue()                                  */
/**************************************************************************/
/* DESCRIPTION: Reads the value in a specified register.                  */
/*                                                                        */
/* INPUT:       regAddr - address of the register                         */
/*                                                                        */
/* OUTPUT:      the value that was read from the register.                */
/*                                                                        */
/**************************************************************************/
static unsigned long bbuBootGetRegValue(unsigned long regAddr)
{
    unsigned long val;

    val = *(volatile unsigned long*)(regAddr);
    if (BBU_HAL_DEBUG)
    {
        printf("%s: read register at offset 0x%X, value is 0x%x\n", __FUNCTION__, regAddr, val);
    }
    return(unsigned long)val;
}

/**************************************************************************/
/*                  bbuBootSetRegValue()                                  */
/**************************************************************************/
/* DESCRIPTION: Writes a given value to a specified register.             */
/*                                                                        */
/* INPUT:       regAddr - address of the register.                        */
/*              val     - value to write to the register.                 */
/*                                                                        */
/* OUTPUT:      none                                                      */
/*                                                                        */
/**************************************************************************/
static void bbuBootSetRegValue(unsigned long regAddr, unsigned long val)
{
    (*(volatile unsigned int *) (regAddr)) = (unsigned long)val;
    if (BBU_HAL_DEBUG)
    {
        printf("%s: write register at offset 0x%X, value is 0x%x\n", __FUNCTION__, regAddr, val);
    }

    return;
}
/**************************************************************************/
/*                        bbuBootAdcSetToIdle()                           */
/**************************************************************************/
/* DESCRIPTION: Sets the ADC to Idle mode.                                */
/*              Note - The function polls the ADC status, and returns     */  
/*                     only after the ADC is in Idle mode, or if a        */
/*                     polling-timeout expires.                           */
/*                                                                        */
/* INPUT:       none                                                      */
/*                                                                        */
/* OUTPUT:      0  - ADC has been set to Idle mode                       */
/*              -1 - Polling timeout has expired                         */
/**************************************************************************/
static int bbuBootAdcSetToIdle(void)
{
    int count = 0;
    unsigned long regVal;

    regVal = bbuBootGetRegValue(BBU_MODECR_PUMA5);
    if (BBU_HAL_DEBUG)
        printf("%s: read register at BBU_MODECR_PUMA5, value is 0x%x\n", __FUNCTION__, regVal);

    if ((regVal & BBU_MODECR_PUMA5_IDLE_MASK))
        return 0;

    /* setting the "stop" bit */
    regVal |= BBU_MODECR_PUMA5_STOP_MASK;

    bbuBootSetRegValue(BBU_MODECR_PUMA5, regVal);

    /* polling on the Idle bit */
    while (count < MAX_ADC_IDLE_POLL*2)
    {
        count++;
        regVal = bbuBootGetRegValue(BBU_MODECR_PUMA5);
        if (regVal & BBU_MODECR_PUMA5_IDLE_MASK)
        {
            if (BBU_HAL_DEBUG)
                printf("%s: ADC set to Idle mode\n", __FUNCTION__);

            return 0;
        }
    }

    if (BBU_HAL_DEBUG)
        printf("%s: failed to set ADC to Idle mode, count = %d\n", __FUNCTION__, count);

    return -1;
}


/**************************************************************************/
/*                     bbuBootSetUpSingleSample()                         */
/**************************************************************************/
/* DESCRIPTION: Sets the ADC to perform one sample on a single channel.   */
/*              Note: This function sets up the ADC, but does not retrieve*/
/*                    the sampled value.                                  */
/*                                                                        */
/* INPUT:       channel - the index of the channel to be sampled.         */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - invalid channel number or other failure.            */
/**************************************************************************/
static int bbuBootSetUpSingleSample(int channel)
{
    unsigned long regVal;

    /* make sure ADC is idle */
    if (bbuBootAdcSetToIdle() == -1)
    {
        return -1;
    }

    /* set the channel to be sampled */
    bbuBootSetRegValue(BBU_ADCCHNL_PUMA5, channel);

    regVal =  bbuBootGetRegValue(BBU_MODECR_PUMA5);
    regVal &= (BBU_MODECR_PUMA5_AVARAGE_MASK | BBU_MODECR_PUMA5_AVGNUM_MASK);
    regVal |= BBU_MODECR_PUMA5_SINGLE_MASK;

    bbuBootSetRegValue(BBU_MODECR_PUMA5, regVal);

    if (BBU_HAL_DEBUG)
        printf("%s: set up single sample on channel #%d\n", __FUNCTION__, channel);


    return 0;
}


/**************************************************************************/
/*                   bbuBootSetUpSingleSampleAndPoll()                    */
/**************************************************************************/
/* DESCRIPTION: Sets the ADC to perform one sample on a single channel,   */
/*              polls the valid bit until data is ready, and returns the  */
/*              sampled value.                                            */
/*                                                                        */
/* INPUT:       channel - the index of the channel to be sampled.         */
/*              pData   - pointer to where the sampled data should be     */
/*                        stored.                                         */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - invalid channel number or other failure.            */
/**************************************************************************/
static int bbuBootSetUpSingleSampleAndPoll(int channel, unsigned short *pData)
{
    unsigned long regVal;
    int count = 0, idleCount = 0;

    if (bbuBootSetUpSingleSample(channel) == -1)
    {
        return -1;
    }

    while (count < MAX_ADC_SAMPLE_POLL)
    {
        regVal = bbuBootGetRegValue(BBU_ADCDATA_PUMA5);

        if (regVal&BBU_ADCDATA_PUMA5_VALID_MASK)
        {
            (*pData) = (unsigned short)(regVal & BBU_ADCDATA_PUMA5_DATA_MASK);

            if (BBU_HAL_DEBUG)
                printf("%s: single sample retrieved, channel = %d ; value = 0x%x ; count = 0x%x\n", __FUNCTION__, channel, (*pData), count);


            /* making sure that the controller returns to Idle state */
            regVal = 0;
            while ((!regVal) && (idleCount < MAX_ADC_SAMPLE_POLL))
            {
                regVal = bbuBootGetRegValue(BBU_MODECR_PUMA5);
                regVal &= BBU_MODECR_PUMA5_IDLE_MASK;
                idleCount++;
            }

            if (idleCount>=MAX_ADC_SAMPLE_POLL)
            {
                return -1;
            }

            return 0;
        }
        count++;
    }
    return -1;
}


/**************************************************************************/
/*                    bbuBootSetUpOneShotSample()                         */
/**************************************************************************/
/* DESCRIPTION: Set the ADC to perform a sample on each channel (one-Shot)*/
/*              Note: This function sets up the ADC, but does not retrieve*/
/*                    the sampled data.                                   */
/*                                                                        */
/* INPUT:       none.                                                     */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - invalid channel number or other failure.            */
/**************************************************************************/
static int bbuBootSetUpOneShotSample(void)
{
    unsigned long regVal;

    /* make sure ADC is idle */
    if (bbuBootAdcSetToIdle() == -1) return -1;

    regVal =  bbuBootGetRegValue(BBU_MODECR_PUMA5);
    regVal &= (BBU_MODECR_PUMA5_AVARAGE_MASK | BBU_MODECR_PUMA5_AVGNUM_MASK);
    regVal |= BBU_MODECR_PUMA5_ONE_SHOT_MASK;

    bbuBootSetRegValue(BBU_MODECR_PUMA5,regVal);

    if (BBU_HAL_DEBUG)
        printf("%s: set up one-shot sample\n", __FUNCTION__);


    return 0;
}

/**************************************************************************/
/*                      bbuBootOneShotSamplePoll()                        */
/**************************************************************************/
/* DESCRIPTION: Polls the valid bit until data is ready, and returns the  */
/*              sampled value.                                            */
/*                                                                        */
/* INPUT:       pData   - pointer to where the sampled data should be     */
/*                        stored.                                         */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - failure.                                            */
/**************************************************************************/
static int bbuBootOneShotSamplePoll(ONE_SHOT_DATA_TYPE *pData)
{
    unsigned long regVal;
    int count = 0, channel, idleCount = 0;

    if (BBU_HAL_DEBUG)
        printf("starting one shot sample \n");


    while (count < MAX_ADC_SAMPLE_POLL)
    {
        regVal = bbuBootGetRegValue(BBU_ONE_SHOT_VALID_REG_PUMA5);

        if (regVal & BBU_ONE_SHOT_VALID_PUMA5_MASK)
        {
            for (channel = 0; channel <= MAX_ADC_CHANNEL_INDEX; channel++)
            {
                regVal = bbuBootGetRegValue(BBU_ONE_SHOT_CHANNEL_REG_PUMA5(channel));
                (pData->vals)[channel] = (unsigned short)(regVal&BBU_ONE_SHOT_PUMA5_DATA_MASK);
            }

            if (BBU_HAL_DEBUG)
                printf("%s: one-Shot sample retrieved\n", __FUNCTION__);


            /* making sure that the controller returns to Idle state */
            regVal = 0;
            while ((!regVal) && (idleCount < MAX_ADC_SAMPLE_POLL))
            {
                regVal = bbuBootGetRegValue(BBU_MODECR_PUMA5);
                regVal &= BBU_MODECR_PUMA5_IDLE_MASK;
                idleCount++;
            }
            if (BBU_HAL_DEBUG)
                printf("%s: controller returned to idle? count %d\n", __FUNCTION__, idleCount);

            if (idleCount>=MAX_ADC_SAMPLE_POLL)
                return -1;

            return 0;
        }
    }
    if (BBU_HAL_DEBUG)
        printf("%s: one-Shot sample polled up to maximum, failing !\n", __FUNCTION__);

    return -1;
}

/**************************************************************************/
/*                  bbuBootSetUpOneShotSampleAndPoll()                    */
/**************************************************************************/
/* DESCRIPTION: Sets the ADC to perform one-shot sampling.                */
/*              polls the valid bit until data is ready, and returns the  */
/*              sampled value.                                            */
/*                                                                        */
/* INPUT:       pData   - pointer to where the sampled data should be     */
/*                        stored.                                         */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - failure.                                            */
/**************************************************************************/
static int bbuBootSetUpOneShotSampleAndPoll(ONE_SHOT_DATA_TYPE *pData)
{
    if (bbuBootSetUpOneShotSample() == -1)
    {
        if (BBU_HAL_DEBUG)
            printf("\n bbuBootSetUpOneShotSampleAndPoll: bbuBootSetUpOneShotSample failed \n");

        return -1;
    }

    return bbuBootOneShotSamplePoll(pData);
}

/**************************************************************************/
/*                  bbuBootSetAveragingMode()                             */
/**************************************************************************/
/* DESCRIPTION: Configures the BBU to a specified averaging mode          */
/*              Note: Updates the continuous-limits after setting the new */
/*                    averaging mode.                                     */
/*                                                                        */
/* INPUT:       mode - Requested averaging mode.                          */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - undefined input value.                              */
/**************************************************************************/
static int bbuBootSetAveragingMode(BBU_AVERAGING_MODE_TYPE mode)
{
    unsigned long regVal;
    BBU_AVERAGING_MODE_TYPE oldMode;


    /* get the current mode */
    regVal = bbuBootGetRegValue(BBU_MODECR_PUMA5); 
    if (BBU_HAL_DEBUG)
        printf("%s: read register at offset BBU_MODECR_PUMA5, value is 0x%x\n", __FUNCTION__, regVal);

    if (!(regVal&BBU_MODECR_PUMA5_AVARAGE_MASK))
        oldMode = AVERAGING_MODE_1;
    else
        oldMode = (regVal&BBU_MODECR_PUMA5_AVGNUM_MASK)>>BBU_MODECR_PUMA5_AVGNUM_SHIFT;

    /* check if the mode needs to be changed */
    if (oldMode == mode)
    {
        if (BBU_HAL_DEBUG)
            printf("%s:  new averaging mode is the same as previous mode.\n", __FUNCTION__);

        return 0;
    }


    if (mode == AVERAGING_MODE_1) /* no averaging */
    {
        regVal &= (~BBU_MODECR_PUMA5_AVARAGE_MASK);
        bbuBootSetRegValue(BBU_MODECR_PUMA5,regVal);
        if (BBU_HAL_DEBUG)
            printf("%s: set averaging mode\n", __FUNCTION__);


        return 0;
    }

    regVal |= BBU_MODECR_PUMA5_AVARAGE_MASK;         /* turn averaging on                     */
    regVal &= (~BBU_MODECR_PUMA5_AVGNUM_MASK);       /* set averaging bits to 0.              */
    regVal |= (mode<<BBU_MODECR_PUMA5_AVGNUM_SHIFT); /* set averaging bits to requested mode. */
    bbuBootSetRegValue(BBU_MODECR_PUMA5,regVal);

    if (BBU_HAL_DEBUG)
        printf("%s: Set averaging mode\n", __FUNCTION__);


    return 0;
}

/**************************************************************************/
/*                  bbuBootSetBbuClockDivider()                           */
/**************************************************************************/
/* DESCRIPTION: Configures the BBU clock divider to a given value.        */
/*              Note: in order to set the divider the BBU clock must be   */
/*              disabled (enable bit = 0).                                */
/*                                                                        */
/* INPUT:       divider - Requested divider value.                        */
/*                                                                        */
/* OUTPUT:      0  - success.                                            */
/*              -1 - undefined input value.                              */
/**************************************************************************/
static int bbuBootSetBbuClockDivider(unsigned short divider)
{
    unsigned long regVal;

    if ((divider < PCLKCR1_PUMA5_BBU_CLK_DIV_MIN) ||
        (divider > PCLKCR1_PUMA5_BBU_CLK_DIV_MAX))
    {
        if (BBU_HAL_DEBUG)
            printf("%s: invalid divider value (%d)\n", __FUNCTION__, divider);

        return -1;
    }

    regVal = bbuBootGetRegValue(BBU_CR);
    regVal &= (~PCLKCR1_PUMA5_BBU_CLK_DIV_MASK);     /* set divider bits to 0.               */
    regVal |= (divider<<BBU_CR_DIV_SHIFT);           /* set divider bits to requested value. */
    bbuBootSetRegValue(BBU_CR, regVal);

    if (BBU_HAL_DEBUG)
        printf("%s: set clock divider to value 0x%X\n", __FUNCTION__, divider);


    return 0;
}

static int bbuBootEnableBattery(unsigned short index, unsigned short enable)
{
    unsigned long bbuCrVal;

    if ((index != BBU_EN_BAT1_INDEX) && (index != BBU_EN_BAT2_INDEX))
        return -1;
    if ((enable !=0) && (enable != 1))
        return -1;

    bbuCrVal = bbuBootGetRegValue(BBU_CR);
    if (index == BBU_EN_BAT1_INDEX)
    {
        if (enable == 1)
        {
            bbuCrVal |= BBU_EN_BAT1_MASK;
        }
        else /*enable = 0*/
        {
            bbuCrVal &= ~BBU_EN_BAT1_MASK;
        }
    }
    else/*BBU_EN_BAT2_INDEX*/
    {
        if (enable == 1)
        {
            bbuCrVal |= BBU_EN_BAT2_MASK;
        }
        else /*enable = 0*/
        {
            bbuCrVal &= ~BBU_EN_BAT2_MASK;
        }
    }

    bbuBootSetRegValue((unsigned long)BBU_CR, bbuCrVal);
    if (BBU_HAL_DEBUG)
    {
        printf("\n battery %d %s \n", index + 1, (enable == 1) ? "enabled" : "disabled");
    }

    return 0;
}


/**************************************************************************/
/*                    bbuBootAveragingCompensation()                      */
/**************************************************************************/
/* DESCRIPTION: Compensating for the averaging performed by the ADC logic.*/
/*              Can convert an averaged value to an equivalent 12bit (not */
/*              averaged) value, or the other way around, depending on    */
/*              required direction.                                       */
/*                                                                        */
/* INPUT:       value - the value to be converted.                        */
/*              dir - direction: 0 = from averaged to 'pure'.             */
/*                               1 = from 'pure' to averaged.             */
/*                                                                        */
/* OUTPUT:      converted value.                                          */
/**************************************************************************/
static unsigned short bbuBootAveragingCompensation(unsigned short value, int dir)
{
    unsigned short averagingShift;
    BBU_AVERAGING_MODE_TYPE averagingMode = AVERAGING_MODE_16;
    unsigned short retVal;

    averagingShift = averagingMode+1;

    if (dir == 0) /* from averaged sample to pure value */
        retVal = value>>averagingShift;
    else /* from averaged sample to pure value */
        retVal = value<<averagingShift;

    return retVal;
}


/**************************************************************************/
/*                   bbuBootConvertHwToPure_Single()                      */
/**************************************************************************/
/* DESCRIPTION: The SW considers all values to be "pure", meaning that    */
/*              they are not effected by calibration figures and          */
/*              averaging mode. However, in the HW logic, the values used */
/*              are effected by these factors.                            */
/*              This function translates a HW value to a "pure" value.    */
/*                                                                        */
/* INPUT:       hwValue - the HW value to be converted.                   */
/*                                                                        */
/* OUTPUT:      converted value.                                          */
/**************************************************************************/
static unsigned short bbuBootConvertHwToPure_Single(unsigned short hwValue)
{
    unsigned short retVal;

    /* compensate for averaging */
    retVal = bbuBootAveragingCompensation(hwValue, 0 /*averaged to pure*/);

    if (BBU_HAL_DEBUG)
        printf("%s: converted Single-Ended HW value 0x%X to 'pure' value 0x%X\n", __FUNCTION__, hwValue, retVal);


    return retVal;
}

/**************************************************************************/
/*                   bbuBootIsBbuSupported()                              */
/**************************************************************************/
/* DESCRIPTION: Check if BBU is supported on chip                         */
/*                                                                        */
/* INPUT:       NONE                                                      */
/*                                                                        */
/* OUTPUT:      1 if supported, 0 - otherwise                             */
/**************************************************************************/
static int bbuBootIsBbuSupported(void) 
{
    int bbuSupportedReg;
    int supported = 1;

    bbuSupportedReg = bbuBootGetRegValue(BBU_ENABLED_REG_PUMA5);

    if (BBU_HAL_DEBUG)
    {
        printf("%s: read bbu supported 0x%lX, value is 0x%X\n", __FUNCTION__, BBU_ENABLED_REG_PUMA5, bbuSupportedReg);
    }

    if ((bbuSupportedReg) & BBU_ENABLED_MASK)
    {
        if (BBU_HAL_DEBUG)
        {
            printf("%s: bbu not supported !!\n", __FUNCTION__);
        }
        supported = 0;
    }
    else
    {
        if (BBU_HAL_DEBUG)
        {
            printf("%s: bbu  supported !!\n", __FUNCTION__);
        }

    }
    return supported;
}

static int bbuIsPowerReset(void)
{
    int powerResetReg;

    powerResetReg =  bbuBootGetRegValue(P5_RST_TYPE_REG);

    powerResetReg &= POWER_RESET_MASK;

    if (BBU_HAL_DEBUG)
    {
        printf("%s: read power reset reg 0x%lX, value is 0x%X \n", __FUNCTION__, P5_RST_TYPE_REG, powerResetReg);
        printf("%s: last was %s power reset  (%d) \n", __FUNCTION__, (powerResetReg >=0) ? "" : "NOT", powerResetReg);
    }

    return powerResetReg;
}

/**************************************************************************/
/*                    bbuBootConvertHwToPure_Diff()                       */
/**************************************************************************/
/* DESCRIPTION: The SW considers all values to be "pure", meaning that    */
/*              they are not effected by calibration figures and          */
/*              averaging mode. However, in the HW logic, the values used */
/*              are effected by these factors.                            */
/*              This function translates a HW value to a "pure" value.    */
/*              It is used for the ADC's differential channel (#7), and   */
/*              can return negative values.                               */
/*                                                                        */
/* INPUT:       hwValue - the HW value to be converted.                   */
/*                                                                        */
/* OUTPUT:      converted value (returned in mAmp).                                          */
/**************************************************************************/
static int bbuBootConvertHwToPure_Diff(unsigned short hwValue)
{
    int retVal;

    /* compensate for averaging */
    retVal = bbuBootAveragingCompensation(hwValue, 0 /*averaged to pure*/);

    if (BBU_HAL_DEBUG)
    {
        printf("%s: converted differential HW value 0x%X to 'pure' value 0x%X\n", __FUNCTION__, hwValue, retVal);
    }

    return retVal;
}

/**************************************************************************/
/*                   bbuBootCheckIfBatteryPresent()                       */
/**************************************************************************/
/* DESCRIPTION: Check if battery present in board                         */
/*                                                                        */
/* INPUT:       ADC sample                                                */
/*                                                                        */
/* OUTPUT:      0 if battery 1 is present, 1 if only battery 2 is present,*/
/*               -1 if none present                                       */
/**************************************************************************/
static short bbuBootCheckIfBatteryPresent(ONE_SHOT_DATA_TYPE *pBbuData, int *direction, unsigned short *batId, int *batTableIndex)
{
    unsigned short  currentSample;
    int             convertedSample;
    unsigned short  id1Sample; 
    unsigned short  id2Sample;
    unsigned short  batIdSample;
    short batteryEnabledIndex = -1;
    const Bbu_BatterySpecificDataBoot_t *bbuBatterySpecificDataP = NULL;
    int batSpecIndex;

    *batTableIndex = -1;

    /*********************************************************/
    /* decide if may enable a battery according to BBU status */
    /*********************************************************/

    /* Check the current direction. Proceed to check even if not discharging*/
    currentSample = pBbuData->vals[BBU_ADC_CHANNEL_BATT_CURRENT];
    convertedSample = bbuBootConvertHwToPure_Diff(currentSample);

    if (convertedSample > BOOT_BBU_DISCHARGE_CURRENT_SAMPLE_LOW) 
    {
        if (BBU_HAL_DEBUG)
        {
            printf("\n battery not discharging sample %d threshold %d\n", convertedSample, BOOT_BBU_DISCHARGE_CURRENT_SAMPLE_LOW);
        }
        *direction = CURRENT_NOT_DISCHARGE; /* not discharge*/
    }
    else
    {
        if (BBU_HAL_DEBUG)
        {
            printf("\n battery discharging sample %d threshold %d\n", convertedSample, BOOT_BBU_DISCHARGE_CURRENT_SAMPLE_LOW);
        }
        *direction = CURRENT_DISCHARGE; /*  discharge*/
    }

    /* check which battery inserted */
    id1Sample = pBbuData->vals[BBU_ADC_CHANNEL_BP1_ID];
    id2Sample = pBbuData->vals[BBU_ADC_CHANNEL_BP2_ID];

    if (id1Sample < BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD)
    {
        if (BBU_HAL_DEBUG)
        {
            printf("\n battery 1 present sample %d threshold %d \n", id1Sample, BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD);
        }

        batteryEnabledIndex = BBU_EN_BAT1_INDEX;
        batIdSample = bbuBootConvertHwToPure_Single(id1Sample); 
    }
    else
    {
        if (id2Sample < BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD)
        {
            if (BBU_HAL_DEBUG)
            {
                printf("\n battery 2 present sample %d threshold %d \n", id2Sample, BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD );
            }

            batteryEnabledIndex = BBU_EN_BAT2_INDEX;
            batIdSample = bbuBootConvertHwToPure_Single(id2Sample); 
        }
        else
        {
            if (BBU_HAL_DEBUG)
            {
                printf("\n no battery present sample1 %d sample 2 %d threshold %d \n", id1Sample, id2Sample, BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD );
            }
        }
    }

    /*determine bat id*/
    if (batteryEnabledIndex != -1) /* battery present, check the ID*/
    {
        for (batSpecIndex = 0; batSpecIndex <BBU_NUM_SUPPORTED_BATTERY_TYPES; batSpecIndex++)
        {
            bbuBatterySpecificDataP = &(Bbu_BatterySpecificData_array[batSpecIndex]);
            if (BBU_HAL_DEBUG)
            {
                printf("\nbattery tableIndex %d id  %d  \n", batSpecIndex,  bbuBatterySpecificDataP->batId);
            }
            if ((batIdSample >= (bbuBatterySpecificDataP->batId - BBU_ID_LINE_THRESHOLD) ) &&
                (batIdSample <= (bbuBatterySpecificDataP->batId + BBU_ID_LINE_THRESHOLD) ))
            {
                if (BBU_HAL_DEBUG)
                {
                    printf("\nbattery id detected %d tableIndex %d \n", bbuBatterySpecificDataP->batId, batSpecIndex );
                }
                *batTableIndex = batSpecIndex;

                break;
            }
        }
    }

    return batteryEnabledIndex;
}

/**************************************************************************/
/*                   bbuBootCheckSafety()                                 */
/**************************************************************************/
/* DESCRIPTION: Check safety of the battery                               */
/*                                                                        */
/* INPUT:       ADC sample                                                */
/*                                                                        */
/* OUTPUT:      1 if all safety checks succeed, 0 otherwise               */
/**************************************************************************/
static int bbuBootCheckSafety(ONE_SHOT_DATA_TYPE *pBbuData, int batTableIndex, int direction)
{
    int retval = 1; 

    unsigned short  sample;
    int            currentSample;
    const Bbu_BatterySpecificDataBoot_t *bbuBatteryDataP = &(Bbu_BatterySpecificData_array[batTableIndex]);
    unsigned short tempMax = (direction == CURRENT_DISCHARGE) ? bbuBatteryDataP->bbuBatMaxTempLimitDischarge  : bbuBatteryDataP->bbuBatMaxTempLimitCharge;
    unsigned short tempMin = (direction == CURRENT_DISCHARGE) ? bbuBatteryDataP->bbuBatMinTempLimitDischarge  : bbuBatteryDataP->bbuBatMinTempLimitCharge;
    unsigned short voltMax = bbuBatteryDataP->bbuBatMaxVoltLimit;
    unsigned short currentMax = (direction == CURRENT_DISCHARGE) ? bbuBatteryDataP->bbuBatMaxCurrLimitDischarge  : (bbuBatteryDataP->bbuBatMaxCurrLimitCharge);

    do
    {
        /* check temperature limits */
        sample = pBbuData->vals[BBU_ADC_CHANNEL_TEMPERATURE];
        sample = bbuBootConvertHwToPure_Single(sample); 

        if (BBU_IS_TEMPERATURE_ABOVE_THRESHOLD(sample, tempMax))
        {
            retval = 0;
            if (BBU_HAL_DEBUG)
                printf("%s: TEMPERATURE_ABOVE_THRESHOLD sample 0x%X treshold 0x%X\n", __FUNCTION__, sample, tempMax);

            break; 
        }

        if (BBU_IS_TEMPERATURE_BELOW_THRESHOLD(sample, tempMin))
        {
            retval = 0;
            if (BBU_HAL_DEBUG)
                printf("%s: TEMPERATURE_BELOW_THRESHOLD sample 0x%X treshold 0x%X\n", __FUNCTION__, sample, tempMin);

            break; 
        }
        if (BBU_HAL_DEBUG)
         {
            printf("%s: Temperature  sample 0x%X \n", __FUNCTION__, sample);
        }

        /*****************************/
        /*   Check Battery Voltage   */
        /*****************************/

        sample = pBbuData->vals[BBU_ADC_CHANNEL_BATT_VOLTAGE];
        sample = bbuBootConvertHwToPure_Single(sample); 

        if (sample > voltMax)
        {
            retval = 0;
            if (BBU_HAL_DEBUG)
            {
                printf("%s: Voltage Beyond threshold  sample 0x%X treshold 0x%X\n", __FUNCTION__, sample, voltMax);
            }

            break; 
        }
        else
        {
            if (BBU_HAL_DEBUG)
            {
                printf("%s: Voltage  sample 0x%X \n", __FUNCTION__, sample);
            }
        }

        /*****************************/
        /*   Check Battery Current   */
        /*****************************/

        sample = pBbuData->vals[BBU_ADC_CHANNEL_BATT_CURRENT];
        currentSample = bbuBootConvertHwToPure_Diff(sample);
        if (BBU_HAL_DEBUG)
        {
            printf("%s: Current:  direction %d charge thresh %d  disch thre %d sample %d\n", __FUNCTION__, direction,
               bbuBatteryDataP->bbuBatMaxCurrLimitCharge,  bbuBatteryDataP->bbuBatMaxCurrLimitDischarge, currentSample);
        }

        if (direction != CURRENT_DISCHARGE)
        {
            if (currentSample >= currentMax)
                {
                    retval = 0;
                    if (BBU_HAL_DEBUG)
                        printf("%s: Current (charge) Beyond threshold  sample %d threshold %d\n", __FUNCTION__, currentSample, currentMax);

                    break; 
                }
        }
        else
        {
            if ((currentSample) < currentMax)
                {
                    retval = 0;
                    if (BBU_HAL_DEBUG)
                        printf("%s: Current (discharge) Beyond threshold  sample %d threshold %d\n", __FUNCTION__, currentSample, currentMax);

                    break; 
                }
        }

        if (BBU_HAL_DEBUG)
        {
            printf("%s: Current  sample %d (hex 0x%X) \n", __FUNCTION__, currentSample, currentSample);
        }

    }
    while (0);


    if (BBU_HAL_DEBUG)
    {
        printf("%s: retval at end %d \n", __FUNCTION__, retval);
    }

    return retval;
}



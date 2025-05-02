/*
 * bbu_boot_params.h
 * Description:
 * BBU definitions to be used in bootloader only.
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

/***************************************************************************/
/*! \file bbu_boot_params.h
 *  /brief BBU definitions to be used in bootloader only
****************************************************************************/

#ifndef _BBU_BOOT_PARAMS_H_
#define _BBU_BOOT_PARAMS_H_


/**************************************************************************/
/*      INCLUDES                                                          */
/**************************************************************************/

/**************************************************************************/
/*      INTERFACE MACRO Definitions                                       */
/**************************************************************************/

/*! \def LPSC_BBU
 *  \brief BBU module enumeration 
 *  for taking it out of reset
 */
#define  LPSC_BBU 9 


/*! \def ADC ports assignment on P5 board
 *  \brief BBU module  
 */
#define BBU_ADC_CHANNEL_BUCK_VOLTAGE  /**/       6      /**/        /* Buck voltage         */
#define BBU_ADC_CHANNEL_BP2_ID        /**/       1      /**/        /* Battery Pack 2 ID    */
#define BBU_ADC_CHANNEL_BP1_ID        /**/       3      /**/        /* Battery Pack 1 ID    */
#define BBU_ADC_CHANNEL_TEMPERATURE   /**/       2      /**/        /* Temperature          */
#define BBU_ADC_CHANNEL_BATT_CURRENT  /**/       7      /**/        /* Battery current      */
#define BBU_ADC_CHANNEL_BATT_VOLTAGE  /**/       5      /**/        /* Battery voltage      */
#define BBU_ADC_CHANNEL_RESERVED1     /**/       0      /**/        /* reserved #2          */
#define BBU_ADC_CHANNEL_RESERVED2     /**/       4      /**/        /* reserved #3          */

#define MAX_ADC_CHANNEL_INDEX                    7

/*! \def BBU_ADC_MAX_CODEWORD
 *  \brief Each ADC voltage measurement is presented in 12 bits. 
 *  The actual max value presented is 12**2 -1 == 4095, 
 *  So all the values are presented as 0 -4095  
 */
#define BBU_ADC_MAX_CODEWORD  4095

/*! \def BBU_REGISTER_BASE_PUMA5
 *  \brief The actual address of the register  
 */
#define BBU_REGISTER_BASE_PUMA5  (0x08611C00)


/******************************************************************************
 *                                  BBU ADC Mode                              *
 ******************************************************************************/

/*! \def BBU_ADC_MODE_TYPE
 *  \brief ADC Operational modes  
 */
typedef enum {
    ADC_MODE_IDLE = 0,
    ADC_MODE_DFT,
    ADC_MODE_CONT,
    ADC_MODE_ONE_SHOT,
    ADC_MODE_SINGLE
}BBU_ADC_MODE_TYPE; 

/*! \def BBU_ADC_MODE_TYPE
 *  \brief ADC averaging modes  
 */
typedef enum {
    AVERAGING_MODE_2 = 0,
    AVERAGING_MODE_4,
    AVERAGING_MODE_8,
    AVERAGING_MODE_16,
    AVERAGING_MODE_1
}BBU_AVERAGING_MODE_TYPE; 

/*! \def BBU_AVERAGING_TYPE_2_SHIFT
 *  \brief conversion from "BBU_AVERAGING_MODE_TYPE" to the shift 
 *   needed in order to compensate for averaging.
 *  e.g.:
 *  BBU_AVERAGING_TYPE_2_SHIFT(AVERAGING_MODE_8) = 3
 *  8 averaged samples must be shifted by 3        
 */
#define BBU_AVERAGING_TYPE_2_SHIFT(x) (((x)+1)%5)

/*! \def BBU_D2S_GAIN_TYPE
 *  \brief Differential to single-ended amplifier gain levels.
 */
typedef enum {
	BBU_D2S_GAIN_1,
	BBU_D2S_GAIN_10,
	BBU_D2S_GAIN_15,
	BBU_D2S_GAIN_20
}BBU_D2S_GAIN_TYPE;

/*! \def MAX_ADC_IDLE_POLL
 *  \brief max number polling to verify ADC returned to idle 
 */
#define MAX_ADC_IDLE_POLL                   10000 

/*! \def MAX_ADC_SAMPLE_POLL
 *  \brief max number polling to verify a valid sample is present
 */
#define MAX_ADC_SAMPLE_POLL                 10000 

/*! \def BBU_MODECR_PUMA5
 *  \brief BBU mode register address
 */
#define   BBU_MODECR_PUMA5            BBU_REGISTER_BASE_PUMA5+0x4

/*! \def BBU_MODECR_PUMA5_...SHIFT
 *  \brief BBU mode shifts used for setting/getting the appropriate working mode
 */
#define BBU_MODECR_PUMA5_IDLE_SHIFT         0
#define BBU_MODECR_PUMA5_STOP_SHIFT         3
#define BBU_MODECR_PUMA5_DFT_SHIFT          4
#define BBU_MODECR_PUMA5_CONT_SHIFT         5
#define BBU_MODECR_PUMA5_ONE_SHOT_SHIFT     6
#define BBU_MODECR_PUMA5_SINGLE_SHIFT       7
#define BBU_MODECR_PUMA5_AVARAGE_SHIFT      8
#define BBU_MODECR_PUMA5_AVGNUM_SHIFT       9

/*! \def BBU_MODECR_PUMA5_...MASK
 *  \brief BBU mode masks used for setting/getting the appropriate working mode
 */
#define BBU_MODECR_PUMA5_IDLE_MASK          (0x1<<BBU_MODECR_PUMA5_IDLE_SHIFT)
#define BBU_MODECR_PUMA5_STOP_MASK          (0x1<<BBU_MODECR_PUMA5_STOP_SHIFT)
#define BBU_MODECR_PUMA5_DFT_MASK           (0x1<<BBU_MODECR_PUMA5_DFT_SHIFT)
#define BBU_MODECR_PUMA5_CONT_MASK          (0x1<<BBU_MODECR_PUMA5_CONT_SHIFT)
#define BBU_MODECR_PUMA5_ONE_SHOT_MASK      (0x1<<BBU_MODECR_PUMA5_ONE_SHOT_SHIFT)
#define BBU_MODECR_PUMA5_SINGLE_MASK        (0x1<<BBU_MODECR_PUMA5_SINGLE_SHIFT)
#define BBU_MODECR_PUMA5_AVARAGE_MASK       (0x1<<BBU_MODECR_PUMA5_AVARAGE_SHIFT)
#define BBU_MODECR_PUMA5_AVGNUM_MASK        (0x3<<BBU_MODECR_PUMA5_AVGNUM_SHIFT)
                                              
/******************************************************************************
 *                                  ADC Analog Control                        *
 ******************************************************************************/

/*! \def BBU_ACTRL_PUMA5
 *  \brief BBU ctrl register
 */
#define BBU_ACTRL_PUMA5                     BBU_REGISTER_BASE_PUMA5+0x8     
        
#define BBU_ACTRL_PUMA5_MODE_SHIFT          0
#define BBU_ACTRL_PUMA5_BPASS_IBUF_SHIFT    1
#define BBU_ACTRL_PUMA5_PWDN_DBUF_SHIFT     2
#define BBU_ACTRL_PUMA5_DIFF_SHIFT          3
#define BBU_ACTRL_PUMA5_SELCALIN_SHIFT      4
#define BBU_ACTRL_PUMA5_CALMODE_SHIFT       5
#define BBU_ACTRL_PUMA5_GAINSEL_SHIFT       12

#define BBU_ACTRL_PUMA5_MODE_MASK           (0x1<<BBU_ACTRL_PUMA5_MODE_SHIFT)
#define BBU_ACTRL_PUMA5_BPASS_IBUF_MASK     (0x1<<BBU_ACTRL_PUMA5_BPASS_IBUF_SHIFT)                                                                
#define BBU_ACTRL_PUMA5_PWDN_DBUF_MASK      (0x1<<BBU_ACTRL_PUMA5_PWDN_DBUF_SHIFT)
#define BBU_ACTRL_PUMA5_DIFF_MASK           (0x1<<BBU_ACTRL_PUMA5_DIFF_SHIFT)
#define BBU_ACTRL_PUMA5_SELCALIN_MASK       (0x1<<BBU_ACTRL_PUMA5_SELCALIN_SHIFT)
#define BBU_ACTRL_PUMA5_CALMODE_MASK        (0x1<<BBU_ACTRL_PUMA5_CALMODE_SHIFT)
#define BBU_ACTRL_PUMA5_GAINSEL_MASK        (0x3<<BBU_ACTRL_PUMA5_GAINSEL_SHIFT)

/******************************************************************************
 *                             ADC Single and DFT Mode                        *
 ******************************************************************************/

/*! \def BBU_ADCCHNL_PUMA5
 *  \brief BBU ADC channel choice
 */
#define BBU_ADCCHNL_PUMA5                   BBU_REGISTER_BASE_PUMA5+0x60     

#define BBU_ADCCHNL_PUMA5_MASK              0x7

/*! \def BBU_ADCDATA_PUMA5
 *  \brief BBU ADC Data  register - for  getting the ADC data status
 */
#define BBU_ADCDATA_PUMA5                   BBU_REGISTER_BASE_PUMA5+0x14

#define BBU_ADCDATA_PUMA5_DATA_SHIFT        0
#define BBU_ADCDATA_PUMA5_VALID_SHIFT       16
#define BBU_ADCDATA_PUMA5_DFT_VIO_SHIFT     17

#define BBU_ADCDATA_PUMA5_DATA_MASK         (0xFFFF << BBU_ADCDATA_PUMA5_DATA_SHIFT)
#define BBU_ADCDATA_PUMA5_VALID_MASK        (0x1 << BBU_ADCDATA_PUMA5_VALID_SHIFT)
#define BBU_ADCDATA_PUMA5_DFT_VIO_MASK      (0x1 << BBU_ADCDATA_PUMA5_DFT_VIO_SHIFT)


/*! \def BBU_SINGLE_DATA_VALID_PUMA5
 *  \brief quick access to check single-mode data validity
 */
#define BBU_SINGLE_DATA_VALID_PUMA5         ( (*(volatile ULONG*)((ULONG)BBU_ADCDATA_PUMA5) & BBU_ADCDATA_PUMA5_VALID_MASK )

/******************************************************************************
 *                                 ADC One-Shot Mode                          *
 ******************************************************************************/

/*! \def BBU_ONE_SHOT_VALID_REG_PUMA5
 *  \brief access to ADC one_shot mode data control
 */
#define  BBU_ONE_SHOT_VALID_REG_PUMA5       BBU_REGISTER_BASE_PUMA5+0x20   

#define BBU_ONE_SHOT_VALID_PUMA5_SHIFT      16
  
#define BBU_ONE_SHOT_VALID_PUMA5_MASK       (0x1 << BBU_ONE_SHOT_VALID_PUMA5_SHIFT)

/*! \def BBU_SINGLE_DATA_VALID_PUMA5
 *  \brief quick access to check one-shot-mode data validity
 */
#define BBU_ONE_SHOT_DATA_VALID_PUMA5       ( (*(volatile ULONG*)((ULONG)BBU_ONE_SHOT_VALID_REG_PUMA5) & BBU_ONE_SHOT_VALID_PUMA5_MASK )

/*! \def BBU_ONE_SHOT_CHANNEL_REG_PUMA5
 *  \brief access to one-shot value registers.
 *  note that that the channel number is assumed to be between 0-7
 */
#define BBU_ONE_SHOT_CHANNEL_REG_PUMA5(x)   BBU_REGISTER_BASE_PUMA5+0x20+(0x8*(x&0x7))

#define BBU_ONE_SHOT_PUMA5_DATA_MASK        0xFFFF

/*! \def ONE_SHOT_DATA_TYPE
 *  \brief container for holding sampled data from all 8 channels
 */
typedef struct{
	unsigned short vals[8];
}ONE_SHOT_DATA_TYPE;

/*! \def BBU Clock Control
 *  \brief definitions to set clock control in Pripheral Clock Control Register #1 
 */
#define BBU_CR                              0x08611b70 /*control register*/
#define PCLKCR1_PUMA5_BBU_CLK_DIV_MASK      0x00000FF0
#define PCLKCR1_PUMA5_BBU_CLK_DIV_MAX       0xFF
#define PCLKCR1_PUMA5_BBU_CLK_DIV_MIN       0xD
#define BBU_CR_DIV_SHIFT                    4

/*! \def BBU_ENABLED_REG_PUMA5
 *  \brief definitions to checking if BBU is enabled on chip 
 */
#define BBU_ENABLED_REG_PUMA5           0x08611a24
#define BBU_ENABLED_MASK                0x8   /*bit 3 – “1” means that the BBU is disabled*/

/*! \def P5_RST_TYPE_REG
 *  \brief register where type of reset is stored 
 */
#define P5_RST_TYPE_REG                  0x086200E4
#define POWER_RESET_MASK                 0x1

/*! \def BBU_EN_BAT1... BBU_EN_BAT2...
 *  \brief indices and masks for battery indicators 
 */
#define BBU_EN_BAT1_INDEX                0 /*The index in BBU_CR reg*/
#define BBU_EN_BAT2_INDEX                1 /*The index in BBU_CR reg*/
#define BBU_EN_BAT1_MASK                 (1<<BBU_EN_BAT1_INDEX) /*0x00000001*/
#define BBU_EN_BAT2_MASK                 (1<<BBU_EN_BAT2_INDEX) /*0x00000002*/


/* When sampling Sense Resistor in order to calculate the current, use this 
   threshold when deciding on current direction (units of ADC sample without 
   averaging).                                                                */

/*! \def BBU_SENSE_CURRENT_VALUE
 *  \brief the value of the current-sense resistor
 */
#define BBU_SENSE_CURRENT_VALUE             0.05 /* ohm */
               
                           #define BBU_IS_TEMPERATURE_ABOVE_THRESHOLD(sample,threshold) (sample<threshold) /*Note - logic is reversed */
#define BBU_IS_TEMPERATURE_BELOW_THRESHOLD(sample,threshold) (sample>threshold) /*Note - logic is reversed */

/*! \def BBU_ADC_CAL_VREF
 *  \brief Vref - ADC reference voltage
 */
#define BBU_ADC_CAL_VREF              1.5

/*! \def BBU_ADC_CAL_VREF_WORD
 *  \brief Vref as it is represented by an ADC sample, in ideal conditions
 */
#define BBU_ADC_CAL_VREF_WORD         0xFFF

/*! \def BBU_ADC_CAL_GAIN_SCALE_SHIFT
 *  \brief used for calibration
 */
#define BBU_ADC_CAL_GAIN_SCALE_SHIFT  16

/*! \def BBU_BATT_VOLTAGE_DIVIDER
 *  \brief ratio betwwen actual battery voltage to he corresponding ADC input (due to
 *  the ratio of resistors)
 */
#define BBU_BATT_VOLTAGE_DIVIDER      9.248 



/*! \def BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD
 *  \brief threshold for determining battery presence
 *    "no battery" measures app 0xFFF0, so this looks like a good threshold
 *     for checking any kind of battery present
 */
#define BOOT_BBU_IS_BATTERY_INSERTED_THRESHOLD 0xF000 


     /* battery specific code  - subset of the one in bbu_bat_cfg.h */

/*! \def BBU_NUM_SUPPORTED_BATTERY_TYPES
 *  \brief number of  battery_types supported
 *  more details (not mandatory)
 */
#define BBU_NUM_SUPPORTED_BATTERY_TYPES 3 

#define BBU_DUMMY (1)

/*! \def BBU_ID_LINE_THRESHOLD
 *  \brief When sampling Battery ID lines, use this threshold to decide if value is valid.
 */
#define BBU_ID_LINE_THRESHOLD                 0x40 


/* macro used to translate a given battery voltage to the corresponding ADC word */
#define BBU_BATT_VOLTAGE_TO_ADC_WORD(x)    ((((x)/BBU_BATT_VOLTAGE_DIVIDER)/BBU_ADC_CAL_VREF)*BBU_ADC_CAL_VREF_WORD)


/* macro used to translate  an ADC word  to battery voltage */
#define BBU_ADC_WORD_TO_BATT_VOLTAGE(x)    (((x) /(BBU_ADC_CAL_VREF_WORD))*(BBU_ADC_CAL_VREF)*(BBU_BATT_VOLTAGE_DIVIDER))

#define BBU_ADC_INPUT_TO_ADC_WORD(arg) ((arg/BBU_ADC_CAL_VREF) * BBU_ADC_CAL_VREF_WORD)
#define BBU_MEASURED_BATTERY_ID(arg) (BBU_ADC_INPUT_TO_ADC_WORD((arg)) )

/*ID line values for supported battery types, [mAh] */
#define BBU_ID_LINE_VAL_2000_COEFF                (1.113)
#define BBU_ID_LINE_VAL_2200_COEFF                (0.552)
#define BBU_ID_LINE_VAL_2400_COEFF                (0.326)

#define BBU_ID_LINE_VAL_2000                ((unsigned short)BBU_MEASURED_BATTERY_ID(BBU_ID_LINE_VAL_2000_COEFF))
#define BBU_ID_LINE_VAL_2200                ((unsigned short)BBU_MEASURED_BATTERY_ID(BBU_ID_LINE_VAL_2200_COEFF))
#define BBU_ID_LINE_VAL_2400                ((unsigned short)BBU_MEASURED_BATTERY_ID(BBU_ID_LINE_VAL_2400_COEFF))


#define INIT_MAXVOLT(maxvolt) (BBU_BATT_VOLTAGE_TO_ADC_WORD(maxvolt))
#define INIT_MINVOLT(minvolt) (BBU_BATT_VOLTAGE_TO_ADC_WORD(minvolt))
#define INIT_MAINTOCHARGE(maintocharge) (BBU_BATT_VOLTAGE_TO_ADC_WORD(maintocharge))
#define INIT_MAXVOLTCHARGE(voltcharge) (BBU_BATT_VOLTAGE_TO_ADC_WORD(voltcharge))

#define INIT_MAXTEMPCHARGE(maxtempcharge)       BBU_ADC_INPUT_TO_ADC_WORD(maxtempcharge)
#define INIT_MINTEMPCHARGE(mintempcharge)       BBU_ADC_INPUT_TO_ADC_WORD(mintempcharge)
#define INIT_MAXTEMPDISCHARGE(maxtempdischarge) BBU_ADC_INPUT_TO_ADC_WORD(maxtempdischarge)
#define INIT_MINTEMPDISCHARGE(mintempdischarge) BBU_ADC_INPUT_TO_ADC_WORD(mintempdischarge)


        /* calculate current threshold - converted to samples*/
#define MILLI_AMPER 1000

/* these Calibration constants are set by the P5 analog team 
    Gain Voltage offset
     1    0.95
    10    1.1
    15    1.15
    20    1.05

*/

/* Note: this gain lets us set both charge and discharge limits. 
If changed - need to change gain setting BBU_DEFAULT_GAIN_FOR_BL as well */

#define BBU_DEFAULT_GAIN_FOR_BL (BBU_D2S_GAIN_10)

/* the values are set according to the table above.*/
#define DEFAULT_GAIN_MULTIPLIER   10
#define VOLTAGE_OFFSET  1.1

/* with gain 10, for 500 mA this is 3686, for -1900 this is 410 */
#define INIT_CURRENT(maxcurrent) ((unsigned short)(((((maxcurrent) * (BBU_SENSE_CURRENT_VALUE) * DEFAULT_GAIN_MULTIPLIER) / MILLI_AMPER) + VOLTAGE_OFFSET) / (BBU_ADC_CAL_VREF) * (BBU_ADC_CAL_VREF_WORD)))

#define BBU_SAFETY_TEMPERATURE_THRESHOLD    0x20

#define BBU_SAFETY_TEMPERATURE_MAX_WITH_THRESHOLD(batPtr)  (batPtr->bbuBatMaxTempLimitCharge + BBU_SAFETY_TEMPERATURE_THRESHOLD)
#define BBU_SAFETY_TEMPERATURE_MIN_WITH_THRESHOLD(batPtr)  (batPtr->bbuBatMinTempLimitCharge - BBU_SAFETY_TEMPERATURE_THRESHOLD)
#define BBU_SAFETY_TEMPERATURE_MAX_WITH_THRESHOLD_DISCHARGE(batPtr)  (batPtr->bbuBatMaxTempLimitDischarge + BBU_SAFETY_TEMPERATURE_THRESHOLD)
#define BBU_SAFETY_TEMPERATURE_MIN_WITH_THRESHOLD_DISCHARGE(batPtr)  (batPtr->bbuBatMinTempLimitDischarge - BBU_SAFETY_TEMPERATURE_THRESHOLD)

#define BBU_SAFETY_VOLTAGE_MAX(batPtr) (batPtr->bbuBatMaxVoltLimit)
#define BBU_BAT_VOLTAGE_MIN(batPtr) (batPtr->bbuBatMinVoltLimit)
#define BBU_SAFETY_CURRENT_MAX_CHARGE(batPtr) (batPtr->bbuBatMaxCurrLimitCharge)
#define BBU_SAFETY_CURRENT_MAX_DISCHARGE(batPtr) (batPtr->bbuBatMaxCurrLimitDischarge)
#define BBU_VOLTAGE_CHARGE_MAX(batPtr) (batPtr->bbuMaxVoltCharge)


#define BBU_SENSE_CURRENT_ZERO              0
#define BBU_SENSE_CURRENT_THRESHOLD        30 /* mA*/

/*! \def BOOT_BBU_DISCHARGE_CURRENT_SAMPLE_LOW
 *  \brief threshold for determining current direction - charging or discharging
 */
/* for gain 10 should be ~ 2963*/
#define BOOT_BBU_DISCHARGE_CURRENT_SAMPLE_LOW  (INIT_CURRENT( ((BBU_SENSE_CURRENT_ZERO) -  (BBU_SENSE_CURRENT_THRESHOLD)) ))
       
/**************************************************************************/
/*      NOTE: only bat 2 - 2200 has valid values !!!!!!!!!!!!!!!!!!!!!!!!!*/
/**************************************************************************/


/*        BAT ID      MAX VOLT     MIN VOLT     MAXTEMP_CHARGE    MINTEMP_CHARGE    MAXTEMP_DISCHARGE     MINTEMP_DISCHARGE    MAXCURR_CHARGE MAXCURR_DISCHARGE      BBU_ID_LINE_VAL   */
/*                        volt        volt       volt(50 degrees)    volt(0 degrees)   volt (60 degrees)    volt (-10 degrees)     mA               mA                    mA           */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
#define BAT_TABLE \
    DESC(BBU_BAT_1,   BBU_DUMMY,     BBU_DUMMY,     BBU_DUMMY,      BBU_DUMMY,          BBU_DUMMY,              BBU_DUMMY,         BBU_DUMMY,      BBU_DUMMY,        BBU_ID_LINE_VAL_2000   )  \
    DESC(BBU_BAT_2,   12.75,         9.2,           0.4518,         1.2748,             0.3816,                 1.4424,             500,           (-1900),           BBU_ID_LINE_VAL_2200   )  \
    DESC(BBU_BAT_3,   BBU_DUMMY,     BBU_DUMMY,     BBU_DUMMY,      BBU_DUMMY,          BBU_DUMMY,              BBU_DUMMY,         BBU_DUMMY,      BBU_DUMMY,        BBU_ID_LINE_VAL_2400   )  


/*! \struct typedef struct Bbu_BatterySpecificData_t
 *  /brief battery specific data- all data relevant to a battery  id
 */
typedef struct
{
    unsigned short batId;               /*  measured bat id */
    unsigned short bbuBatMaxVoltLimit; /* When sampling the Battery Voltage, use this threshold to decide if the 
                                voltage is exceeding the allowed limit.                                    */
    unsigned short bbuBatMinVoltLimit;
    /* When sampling the temperature sensor, use these thresholds to decide if 
                                the temperature is exceeding the allowed limits. */
    unsigned short bbuBatMaxTempLimitCharge;
    unsigned short bbuBatMinTempLimitCharge;
    unsigned short bbuBatMaxTempLimitDischarge;
    unsigned short bbuBatMinTempLimitDischarge;
    unsigned short bbuBatMaxCurrLimitCharge;
    short bbuBatMaxCurrLimitDischarge;

} Bbu_BatterySpecificDataBoot_t;



#endif /*_BBU_BOOT_PARAMS_H_*/

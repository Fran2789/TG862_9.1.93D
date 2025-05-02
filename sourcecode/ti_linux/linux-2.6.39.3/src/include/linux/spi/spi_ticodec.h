#ifndef __TI_CODEC_SPI_H_
#define __TI_CODEC_SPI_H_

struct ti_ctlr_cs_sel_t
{
	u8 cs;
	u8 pol;
};

#define SPI_3_WIRE 0x40

struct ti_codec_spi_platform_data {
	/* board specific information */
	u16 	initial_spmode;
	u16		bus_num; /* id for controller */
	u16		max_chipselect;
	int		(*activate_cs)( u8 cs, u8 polarity, 
                            struct ti_ctlr_cs_sel_t *ctlr_cs_sel);
	int		(*deactivate_cs)( u8 cs, u8 polarity,
                              struct ti_ctlr_cs_sel_t *ctlr_cs_sel);
	u32		sysclk;
};

#endif /* __TI_CODEC_SPI_H_ */

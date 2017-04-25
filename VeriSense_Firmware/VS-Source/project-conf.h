/*
 ******************************************************************************
 *
 *   This File Contains PROPRIETARY and CONFIDENTIAL TRADE SECRET information
 *   which is the exclusive property of VeriSolutions, LLC.  "Trade Secret"
 *   means information constituting a trade secret within the meaning of
 *   Section 10-1-761(4) of the Georgia Trade Secrets Act of 1990, including
 *   all amendments hereafter adopted.
 *
 *   (c) 2017 by VeriSolutions, LLC.                 www.VeriSolutions.co
 *
 ******************************************************************************
 *
 *      VERIRADIO PROJECT CONFIGURATION FILE
 *
 *
 *
 *
 ******************************************************************************
 */

#ifndef		PROJECT_CONF_H_
#define		PROJECT_CONF_H_

#define		RF_CORE_CONF_CHANNEL						26
#define		FW_VERSION									"2.1.0"

//-----------------------------------------------------------------------------

#define MAX_VS_CLIENTS                              30
#define HUB_VS_MAX_WAITING_FOR_ACK                  (CLOCK_SECOND*6)
#define HUB_VS_MAX_RETRANSMITS                      6
#define HUB_COMM_WITH_ELECTRON_INTERVAL             (CLOCK_SECOND*5)
#define SENSOR_COMM_WITH_HUB_INTERVAL               (CLOCK_SECOND*60*30) /*30 min*/
#define SENSOR_INITIAL_COMM_WITH_HUB                (CLOCK_SECOND*60*2) /*2 min*/
#define SENSOR_VS_MAX_RETRANSMITS                   5
#define SENSOR_VS_MAX_WAITING_FOR_ACK               (CLOCK_SECOND*4)
#define SENSOR_RETRY_SENSOR_READ_INTERVAL           (CLOCK_SECOND*2)

//-----------------------------------------------------------------------------

#undef		CONTIKIMAC_CONF_WITH_PHASE_OPTIMIZATION			/* ensure same in */
#define		CONTIKIMAC_CONF_WITH_PHASE_OPTIMIZATION		1	/* contiki-conf.h */
#undef		NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define		NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE		2

//#undef	LLSEC802154_CONF_ENABLED
//#define	LLSEC802154_CONF_ENABLED					1

//#undef	NETSTACK_CONF_FRAMER
//#define	NETSTACK_CONF_FRAMER						noncoresec_framer

//#undef	NETSTACK_CONF_LLSEC
//#define	NETSTACK_CONF_LLSEC							noncoresec_driver

#undef		NONCORESEC_CONF_SEC_LVL
#define		NONCORESEC_CONF_SEC_LVL						0

//#define	NONCORESEC_CONF_KEY							{0x14,0x77,0x65,0xBC,0xAD,0x2D,0xDA,0x90,0x16,0x5A,0x98,0xFC,0xFC,0xB3,0x8E,0x86}

/**
  * Bootloader settings
  */
#define		SET_CCFG_BL_CONFIG_BOOTLOADER_ENABLE		0xC5
#define		SET_CCFG_BL_CONFIG_BL_LEVEL					0x1
#define		SET_CCFG_BL_CONFIG_BL_PIN_NUMBER			0x3
#define		SET_CCFG_BL_CONFIG_BL_ENABLE				0xC5

#endif /* PROJECT_CONF_H_ */

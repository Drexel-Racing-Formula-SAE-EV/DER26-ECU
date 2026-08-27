/**
 ******************************************************************************
  * @file    user_diskio_spi.c
  * @brief   This file contains the implementation of the user_diskio_spi FatFs
  *          driver.
  ******************************************************************************
  * Portions copyright (C) 2014, ChaN, all rights reserved.
  * Portions copyright (C) 2017, kiwih, all rights reserved.
  *
  * This software is a free software and there is NO WARRANTY.
  * No restriction on use. You can use, modify and redistribute it for
  * personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
  * Redistributions of source code must retain the above copyright notice.
  *
  ******************************************************************************
  */

//This code was ported by kiwih from a copywrited (C) library written by ChaN
//available at http://elm-chan.org/fsw/ff/ffsample.zip
//(text at http://elm-chan.org/fsw/ff/00index_e.html)

//This file provides the FatFs driver functions and SPI code required to manage
//an SPI-connected MMC or compatible SD card with FAT

//It is designed to be wrapped by a cubemx generated user_diskio.c file.

#include "stm32f7xx_hal.h" /* Provide the low-level HAL functions */
#include "user_diskio_spi.h"
#include <string.h>

//Make sure you set #define SD_SPI_HANDLE as some hspix in main.h
extern SPI_HandleTypeDef SD_SPI_HANDLE;

/* Function prototypes */

//(Note that the _256 is used as a mask to clear the prescalar bits as it provides binary 111 in the correct position)
#define FCLK_SLOW() do { __HAL_SPI_DISABLE(&SD_SPI_HANDLE); MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_256); __HAL_SPI_ENABLE(&SD_SPI_HANDLE); } while (0) /* 108 MHz / 256 = 421.875 kHz */
#define FCLK_FAST() do { __HAL_SPI_DISABLE(&SD_SPI_HANDLE); MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_CR1_BR, SPI_BAUDRATEPRESCALER_8); __HAL_SPI_ENABLE(&SD_SPI_HANDLE); } while (0) /* 108 MHz / 8 = 13.5 MHz */

#define CS_HIGH()	{HAL_GPIO_WritePin(SPI6_NSS_GPIO_Port, SPI6_NSS_Pin, GPIO_PIN_SET);}
#define CS_LOW()	{HAL_GPIO_WritePin(SPI6_NSS_GPIO_Port, SPI6_NSS_Pin, GPIO_PIN_RESET);}

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */

static volatile
DSTATUS Stat = STA_NOINIT;	/* Physical drive status */

static
BYTE CardType;			/* Card type flags */

static user_spi_sd_diag_t SdDiag = {
    USER_SPI_SD_STAGE_IDLE, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0u, 0u
};

static void sd_diag_reset(void)
{
    SdDiag.stage = USER_SPI_SD_STAGE_IDLE;
    SdDiag.cmd0_r1 = 0xFFu;
    SdDiag.cmd8_r1 = 0xFFu;
    SdDiag.acmd41_or_cmd1_r1 = 0xFFu;
    SdDiag.cmd58_or_cmd16_r1 = 0xFFu;
    SdDiag.card_type = 0u;
    SdDiag.csd_valid = 0u;
}

void USER_SPI_get_diag(user_spi_sd_diag_t *out)
{
    if (out != NULL) { *out = SdDiag; }
}

void USER_SPI_force_not_ready(void)
{
    CardType = 0u;
    Stat = STA_NOINIT;
    SdDiag.card_type = 0u;
    if (SdDiag.stage != USER_SPI_SD_STAGE_IDLE) {
        SdDiag.stage = USER_SPI_SD_STAGE_FAILED;
    }
}

const char *USER_SPI_stage_name(user_spi_sd_stage_t stage)
{
    switch (stage) {
    case USER_SPI_SD_STAGE_IDLE: return "IDLE";
    case USER_SPI_SD_STAGE_STARTUP_CLOCKS: return "CLOCKS";
    case USER_SPI_SD_STAGE_CMD0: return "CMD0";
    case USER_SPI_SD_STAGE_CMD8: return "CMD8";
    case USER_SPI_SD_STAGE_ACMD41_OR_CMD1: return "ACMD41/CMD1";
    case USER_SPI_SD_STAGE_CMD58_OR_CMD16: return "CMD58/CMD16";
    case USER_SPI_SD_STAGE_CSD_VERIFY: return "CSD";
    case USER_SPI_SD_STAGE_READY: return "READY";
    case USER_SPI_SD_STAGE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

uint32_t spiTimerTickStart;
uint32_t spiTimerTickDelay;

void SPI_Timer_On(uint32_t waitTicks) {
    spiTimerTickStart = HAL_GetTick();
    spiTimerTickDelay = waitTicks;
}

uint8_t SPI_Timer_Status() {
    return ((HAL_GetTick() - spiTimerTickStart) < spiTimerTickDelay);
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
static
BYTE xchg_spi (
	BYTE dat	/* Data to send */
)
{
	BYTE rxDat = 0xFFu;
    if (HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &dat, &rxDat, 1u, 50u) != HAL_OK) { return 0xFFu; }
    return rxDat;
}


/* Receive multiple byte */
static
void rcvr_spi_multi (
	BYTE *buff,		/* Pointer to data buffer */
	UINT btr		/* Number of bytes to receive (even number) */
)
{
	for(UINT i=0; i<btr; i++) {
		*(buff+i) = xchg_spi(0xFF);
	}
}


#if _USE_WRITE
/* Send multiple byte */
static
void xmit_spi_multi (
	const BYTE *buff,	/* Pointer to the data */
	UINT btx			/* Number of bytes to send (even number) */
)
{
	(void)HAL_SPI_Transmit(&SD_SPI_HANDLE, (uint8_t *)buff, btx, 500u);
}
#endif


/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (	/* 1:Ready, 0:Timeout */
	UINT wt			/* Timeout [ms] */
)
{
	BYTE d;
	//wait_ready needs its own timer, unfortunately, so it can't use the
	//spi_timer functions
	uint32_t waitSpiTimerTickStart;
	uint32_t waitSpiTimerTickDelay;

	waitSpiTimerTickStart = HAL_GetTick();
	waitSpiTimerTickDelay = (uint32_t)wt;
	do {
		d = xchg_spi(0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multitask envilonment. */
	} while (d != 0xFF && ((HAL_GetTick() - waitSpiTimerTickStart) < waitSpiTimerTickDelay));	/* Wait for card goes ready or timeout */

	return (d == 0xFF) ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Despiselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/

static
void despiselect (void)
{
	CS_HIGH();		/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */

}



/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/

static
int spiselect (void)	/* 1:OK, 0:Timeout */
{
	CS_LOW();		/* Set CS# low */
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/* Wait for card ready */

	despiselect();
	return 0;	/* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (	/* 1:OK, 0:Error */
	BYTE *buff,			/* Data buffer */
	UINT btr			/* Data block length (byte) */
)
{
	BYTE token;


	SPI_Timer_On(200);
	do {							/* Wait for DataStart token in timeout of 200ms */
		token = xchg_spi(0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && SPI_Timer_Status());
	if(token != 0xFE) return 0;		/* Function fails if invalid DataStart token or timeout */

	rcvr_spi_multi(buff, btr);		/* Store trailing data to the buffer */
	xchg_spi(0xFF); xchg_spi(0xFF);			/* Discard CRC */

	return 1;						/* Function succeeded */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
static
int xmit_datablock (	/* 1:OK, 0:Failed */
	const BYTE *buff,	/* Ponter to 512 byte data to be sent */
	BYTE token			/* Token */
)
{
	BYTE resp;


	if (!wait_ready(500)) return 0;		/* Wait for card ready */

	xchg_spi(token);					/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		xmit_spi_multi(buff, 512);		/* Data */
		xchg_spi(0xFF); xchg_spi(0xFF);	/* Dummy CRC */

		resp = xchg_spi(0xFF);				/* Receive data resp */
		if ((resp & 0x1F) != 0x05) return 0;	/* Function fails if the data packet was not accepted */
	}
	return 1;
}
#endif


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/

static
BYTE send_cmd (		/* Return value: R1 resp (bit7==1:Failed to send) */
	BYTE cmd,		/* Command index */
	DWORD arg		/* Argument */
)
{
	BYTE n, res;


	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		despiselect();
		if (!spiselect()) return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);				/* Start + command index */
	xchg_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xchg_spi((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12) xchg_spi(0xFF);	/* Diacard following one byte when CMD12 */
	n = 10;								/* Wait for response (10 bytes max) */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n);

	return res;							/* Return received response */
}


/*--------------------------------------------------------------------------

   Public FatFs Functions (wrapped in user_diskio.c)

---------------------------------------------------------------------------*/

//The following functions are defined as inline because they aren't the functions that
//are passed to FatFs - they are wrapped by autogenerated (non-inline) cubemx template
//code.
//If you do not wish to use cubemx, remove the "inline" from these functions here
//and in the associated .h


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS USER_SPI_initialize (
	BYTE drv		/* Physical drive number (0) */
)
{
	BYTE n, cmd = 0u, ty = 0u, ocr[4] = {0u, 0u, 0u, 0u};
	BYTE csd[16];
	BYTE r1;
	int csd_all_zero = 1;
	int csd_all_ff = 1;

	if (drv != 0u) return STA_NOINIT;

	sd_diag_reset();
	CardType = 0u;
	Stat = STA_NOINIT;

	FCLK_SLOW();
	CS_HIGH();
	HAL_Delay(2u);
	SdDiag.stage = USER_SPI_SD_STAGE_STARTUP_CLOCKS;
	for (n = 10u; n != 0u; n--) xchg_spi(0xFFu); /* 80 clocks with CS high */

	SdDiag.stage = USER_SPI_SD_STAGE_CMD0;
	r1 = send_cmd(CMD0, 0u);
	SdDiag.cmd0_r1 = r1;
	if (r1 != 0x01u) goto failed;

	SPI_Timer_On(1000u);
	SdDiag.stage = USER_SPI_SD_STAGE_CMD8;
	r1 = send_cmd(CMD8, 0x1AAu);
	SdDiag.cmd8_r1 = r1;

	if (r1 == 0x01u) {
		for (n = 0u; n < 4u; n++) ocr[n] = xchg_spi(0xFFu);
		if (ocr[2] != 0x01u || ocr[3] != 0xAAu) goto failed;

		SdDiag.stage = USER_SPI_SD_STAGE_ACMD41_OR_CMD1;
		do {
			r1 = send_cmd(ACMD41, 1UL << 30);
			SdDiag.acmd41_or_cmd1_r1 = r1;
		} while (SPI_Timer_Status() && r1 == 0x01u);
		if (r1 != 0x00u) goto failed;

		SdDiag.stage = USER_SPI_SD_STAGE_CMD58_OR_CMD16;
		r1 = send_cmd(CMD58, 0u);
		SdDiag.cmd58_or_cmd16_r1 = r1;
		if (r1 != 0x00u) goto failed;
		for (n = 0u; n < 4u; n++) ocr[n] = xchg_spi(0xFFu);
		ty = (ocr[0] & 0x40u) ? (CT_SD2 | CT_BLOCK) : CT_SD2;
	} else {
		/* Legacy SDv1/MMC path. Reject impossible/undefined R1 values. */
		if ((r1 & 0x80u) != 0u || r1 == 0x00u) goto failed;

		SdDiag.stage = USER_SPI_SD_STAGE_ACMD41_OR_CMD1;
		r1 = send_cmd(ACMD41, 0u);
		if (r1 <= 0x01u) {
			ty = CT_SD1;
			cmd = ACMD41;
		} else {
			ty = CT_MMC;
			cmd = CMD1;
		}
		do {
			r1 = send_cmd(cmd, 0u);
			SdDiag.acmd41_or_cmd1_r1 = r1;
		} while (SPI_Timer_Status() && r1 == 0x01u);
		if (r1 != 0x00u) goto failed;

		SdDiag.stage = USER_SPI_SD_STAGE_CMD58_OR_CMD16;
		r1 = send_cmd(CMD16, 512u);
		SdDiag.cmd58_or_cmd16_r1 = r1;
		if (r1 != 0x00u) goto failed;
	}

	/* A second, independent proof that a real card exists. A floating MISO line
	 * can occasionally mimic one-byte R1 values, but cannot realistically
	 * produce a valid CMD9 data token plus a nonconstant 16-byte CSD. */
	SdDiag.stage = USER_SPI_SD_STAGE_CSD_VERIFY;
	memset(csd, 0, sizeof(csd));
	if (send_cmd(CMD9, 0u) != 0x00u || !rcvr_datablock(csd, sizeof(csd))) goto failed;
	for (n = 0u; n < sizeof(csd); n++) {
		if (csd[n] != 0x00u) csd_all_zero = 0;
		if (csd[n] != 0xFFu) csd_all_ff = 0;
	}
	if (csd_all_zero || csd_all_ff) goto failed;
	if ((ty & CT_SDC) != 0u && ((csd[0] >> 6) > 1u)) goto failed;

	SdDiag.csd_valid = 1u;
	CardType = ty;
	SdDiag.card_type = ty;
	despiselect();
	FCLK_FAST();
	Stat &= (DSTATUS)~STA_NOINIT;
	SdDiag.stage = USER_SPI_SD_STAGE_READY;
	return Stat;

failed:
	despiselect();
	CardType = 0u;
	SdDiag.card_type = 0u;
	SdDiag.csd_valid = 0u;
	Stat = STA_NOINIT;
	SdDiag.stage = USER_SPI_SD_STAGE_FAILED;
	return Stat;
}


/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS USER_SPI_status (
	BYTE drv		/* Physical drive number (0) */
)
{
	if (drv) return STA_NOINIT;		/* Supports only drive 0 */

	return Stat;	/* Return disk status */
}



/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT USER_SPI_read (
	BYTE drv,		/* Physical drive number (0) */
	BYTE *buff,		/* Pointer to the data buffer to store read data */
	DWORD sector,	/* Start sector number (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
	if (drv || !count) return RES_PARERR;		/* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* LBA ot BA conversion (byte addressing cards) */

	if (count == 1) {	/* Single sector read */
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, 512)) {
			count = 0;
		}
	}
	else {				/* Multiple sector read */
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512)) break;
				buff += 512;
			} while (--count);
			send_cmd(CMD12, 0);				/* STOP_TRANSMISSION */
		}
	}
	despiselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT USER_SPI_write (
	BYTE drv,			/* Physical drive number (0) */
	const BYTE *buff,	/* Ponter to the data to write */
	DWORD sector,		/* Start sector number (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	if (drv || !count) return RES_PARERR;		/* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check drive status */
	if (Stat & STA_PROTECT) return RES_WRPRT;	/* Check write protect */

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* LBA ==> BA conversion (byte addressing cards) */

	if (count == 1) {	/* Single sector write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE)) {
			count = 0;
		}
	}
	else {				/* Multiple sector write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD)) count = 1;	/* STOP_TRAN token */
		}
	}
	despiselect();

	return count ? RES_ERROR : RES_OK;	/* Return result */
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT USER_SPI_ioctl (
	BYTE drv,		/* Physical drive number (0) */
	BYTE cmd,		/* Control command code */
	void *buff		/* Pointer to the conrtol data */
)
{
	DRESULT res;
	BYTE n, csd[16];
	DWORD *dp, st, ed, csize;


	if (drv) return RES_PARERR;					/* Check parameter */
	if (Stat & STA_NOINIT) return RES_NOTRDY;	/* Check if drive is ready */

	res = RES_ERROR;

	switch (cmd) {
	case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
		if (spiselect()) res = RES_OK;
		break;

	case GET_SECTOR_SIZE:
		*(WORD *)buff = 512u;
		res = RES_OK;
		break;

	case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (DWORD) */
		if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
			if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
				csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
				*(DWORD*)buff = csize << 10;
			} else {					/* SDC ver 1.XX or MMC ver 3 */
				n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
				csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
				*(DWORD*)buff = csize << (n - 9);
			}
			res = RES_OK;
		}
		break;

	case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
		if (CardType & CT_SD2) {	/* SDC ver 2.00 */
			if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
				xchg_spi(0xFF);
				if (rcvr_datablock(csd, 16)) {				/* Read partial block */
					for (n = 64 - 16; n; n--) xchg_spi(0xFF);	/* Purge trailing data */
					*(DWORD*)buff = 16UL << (csd[10] >> 4);
					res = RES_OK;
				}
			}
		} else {					/* SDC ver 1.XX or MMC */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
				if (CardType & CT_SD1) {	/* SDC ver 1.XX */
					*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
				} else {					/* MMC */
					*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
				}
				res = RES_OK;
			}
		}
		break;

	case CTRL_TRIM :	/* Erase a block of sectors (used when _USE_ERASE == 1) */
		if (!(CardType & CT_SDC)) break;				/* Check if the card is SDC */
		if (USER_SPI_ioctl(drv, MMC_GET_CSD, csd)) break;	/* Get CSD */
		if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
		dp = buff; st = dp[0]; ed = dp[1];				/* Load sector block */
		if (!(CardType & CT_BLOCK)) {
			st *= 512; ed *= 512;
		}
		if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000)) {	/* Erase sector block */
			res = RES_OK;	/* FatFs does not check result of this command */
		}
		break;

	default:
		res = RES_PARERR;
	}

	despiselect();

	return res;
}
#endif

/************************************************************************************//**
* \file         Source/ARMCM4_STM32F3/can.c
* \brief        Bootloader CAN communication interface source file.
* \ingroup      Target_ARMCM4_STM32F3
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2016  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
* This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You have received a copy of the GNU General Public License along with OpenBLT. It
* should be located in ".\Doc\license.html". If not, contact Feaser to obtain a copy.
*
* \endinternal
****************************************************************************************/


/****************************************************************************************
* Include files
****************************************************************************************/
#include "boot.h"                                /* bootloader generic header          */
#if (BOOT_COM_CAN_ENABLE > 0)
#include "stm32f3xx.h"                           /* STM32 CPU and HAL header           */
#include "stm32f3xx_ll_rcc.h"                    /* STM32 LL RCC header                */


/****************************************************************************************
* Macro definitions
****************************************************************************************/
/** \brief Timeout for transmitting a CAN message in milliseconds. */
#define CAN_MSG_TX_TIMEOUT_MS          (50u)

/** \brief Set CAN base address to CAN1. */
#define CAN_CHANNEL   CAN


/****************************************************************************************
* Type definitions
****************************************************************************************/
/** \brief Structure type for grouping CAN bus timing related information. */
typedef struct t_can_bus_timing
{
  blt_int8u tseg1;                                    /**< CAN time segment 1          */
  blt_int8u tseg2;                                    /**< CAN time segment 2          */
} tCanBusTiming;


/****************************************************************************************
* Local constant declarations
****************************************************************************************/
/** \brief CAN bittiming table for dynamically calculating the bittiming settings.
 *  \details According to the CAN protocol 1 bit-time can be made up of between 8..25
 *           time quanta (TQ). The total TQ in a bit is SYNC + TSEG1 + TSEG2 with SYNC
 *           always being 1. The sample point is (SYNC + TSEG1) / (SYNC + TSEG1 + SEG2) *
 *           100%. This array contains possible and valid time quanta configurations with
 *           a sample point between 68..78%.
 */
static const tCanBusTiming canTiming[] =
{
  /*  TQ | TSEG1 | TSEG2 | SP  */
  /* ------------------------- */
  {  5, 2 },          /*   8 |   5   |   2   | 75% */
  {  6, 2 },          /*   9 |   6   |   2   | 78% */
  {  6, 3 },          /*  10 |   6   |   3   | 70% */
  {  7, 3 },          /*  11 |   7   |   3   | 73% */
  {  8, 3 },          /*  12 |   8   |   3   | 75% */
  {  9, 3 },          /*  13 |   9   |   3   | 77% */
  {  9, 4 },          /*  14 |   9   |   4   | 71% */
  { 10, 4 },          /*  15 |  10   |   4   | 73% */
  { 11, 4 },          /*  16 |  11   |   4   | 75% */
  { 12, 4 },          /*  17 |  12   |   4   | 76% */
  { 12, 5 },          /*  18 |  12   |   5   | 72% */
  { 13, 5 },          /*  19 |  13   |   5   | 74% */
  { 14, 5 },          /*  20 |  14   |   5   | 75% */
  { 15, 5 },          /*  21 |  15   |   5   | 76% */
  { 15, 6 },          /*  22 |  15   |   6   | 73% */
  { 16, 6 },          /*  23 |  16   |   6   | 74% */
  { 16, 7 },          /*  24 |  16   |   7   | 71% */
  { 16, 8 }           /*  25 |  16   |   8   | 68% */
};


/****************************************************************************************
* dingoFW runtime CAN configuration
*----------------------------------------------------------------------------------------
* The application persists its DeviceConfig as a raw struct image at the start of the last
* 2 KB flash sector (the config sector at 0x0800F800 on the CANBoard's STM32F303K8). Its
* first fields are stable and known to the bootloader:
*     offset 0 : uint16  nConfigVersion
*     offset 2 : uint16  nBaseId      (valid 11-bit base, 0..0x7FF)
*     offset 4 : uint8   eCanSpeed    (0=1M, 1=500k, 2=250k, 3=125k, 4=100k)
* (A static_assert in the firmware's core/config.h pins these offsets so an app-side struct
*  change can't silently break this reader.)
*
* Reading them here means a single firmware setting drives both the running app and the
* bootloader: change the module's CAN speed or base ID in dingoConfig and the bootloader
* comes up the same way next time it runs - over the warm-reset path, a cold boot with an
* invalid app, or the backdoor. The XCP message IDs are derived from the base ID so several
* modules coexist on one bus and are flashed individually:
*     XCP cmd  (tool -> bootloader) = base + 12
*     XCP resp (bootloader -> tool) = base + 13
* If the config sector is blank or out of range (fresh SWD-flashed board) the BOOT_COM_CAN_*
* fallback defaults from blt_conf.h are used instead.
****************************************************************************************/
/** \brief Address of the application config sector (CANBoard: last 2 KB of 64 KB flash). */
#define DINGO_CFG_ADDR          (0x0800F800UL)
/** \brief Byte offset of uint16 nBaseId within the persisted config blob. */
#define DINGO_CFG_BASEID_OFS    (2u)
/** \brief Byte offset of uint8 eCanSpeed within the persisted config blob. */
#define DINGO_CFG_CANSPEED_OFS  (4u)
/** \brief XCP command id offset (tool -> bootloader) relative to the base id. */
#define DINGO_XCP_CMD_OFFSET    (12u)
/** \brief XCP response id offset (bootloader -> tool) relative to the base id. */
#define DINGO_XCP_RSP_OFFSET    (13u)

/** \brief Runtime CAN settings, seeded with the blt_conf.h fallbacks. */
static blt_int32u dingoRxMsgId  = BOOT_COM_CAN_RX_MSG_ID;
static blt_int32u dingoTxMsgId  = BOOT_COM_CAN_TX_MSG_ID;
static blt_int32u dingoBaudrate = BOOT_COM_CAN_BAUDRATE;

/************************************************************************************//**
** \brief     Reads the application's persisted base id and CAN speed from the config
**            sector and derives the runtime baudrate and XCP message ids from them.
**            Leaves the blt_conf.h fallback values in place if the config is blank/invalid.
** \return    none.
****************************************************************************************/
static void DingoLoadCanConfig(void)
{
  blt_int16u baseId   = *(volatile const blt_int16u *)(DINGO_CFG_ADDR + DINGO_CFG_BASEID_OFS);
  blt_int8u  canSpeed = *(volatile const blt_int8u  *)(DINGO_CFG_ADDR + DINGO_CFG_CANSPEED_OFS);
  /* index matches the firmware's CanBitrate enum order (see core/enums.h). */
  static const blt_int32u speedTable[5] = { 1000000u, 500000u, 250000u, 125000u, 100000u };

  /* base id must be a standard 11-bit id with room left for base+13. */
  if (baseId <= (0x7FFu - DINGO_XCP_RSP_OFFSET))
  {
    dingoRxMsgId = (blt_int32u)baseId + DINGO_XCP_CMD_OFFSET;
    dingoTxMsgId = (blt_int32u)baseId + DINGO_XCP_RSP_OFFSET;
  }
  if (canSpeed < (sizeof(speedTable)/sizeof(speedTable[0])))
  {
    dingoBaudrate = speedTable[canSpeed];
  }
} /*** end of DingoLoadCanConfig ***/


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/** \brief CAN handle to be used in API calls. */
static CAN_HandleTypeDef canHandle;


/************************************************************************************//**
** \brief     Search algorithm to match the desired baudrate to a possible bus
**            timing configuration.
** \param     baud The desired baudrate in kbps. Valid values are 10..1000.
** \param     prescaler Pointer to where the value for the prescaler will be stored.
** \param     tseg1 Pointer to where the value for TSEG2 will be stored.
** \param     tseg2 Pointer to where the value for TSEG2 will be stored.
** \return    BLT_TRUE if the CAN bustiming register values were found, BLT_FALSE
**            otherwise.
**
****************************************************************************************/
static blt_bool CanGetSpeedConfig(blt_int16u baud, blt_int16u *prescaler,
                                  blt_int8u *tseg1, blt_int8u *tseg2)
{
  blt_int8u  cnt;
  blt_int32u canClockFreqkHz;
  LL_RCC_ClocksTypeDef rccClocks;

  /* read clock frequencies */
  LL_RCC_GetSystemClocksFreq(&rccClocks);
  /* store CAN peripheral clock speed in kHz */
  canClockFreqkHz = rccClocks.PCLK1_Frequency / 1000u;

  /* loop through all possible time quanta configurations to find a match */
  for (cnt=0; cnt < sizeof(canTiming)/sizeof(canTiming[0]); cnt++)
  {
    if ((canClockFreqkHz % (baud*(canTiming[cnt].tseg1+canTiming[cnt].tseg2+1))) == 0)
    {
      /* compute the prescaler that goes with this TQ configuration */
      *prescaler = canClockFreqkHz/(baud*(canTiming[cnt].tseg1+canTiming[cnt].tseg2+1));

      /* make sure the prescaler is valid */
      if ((*prescaler > 0) && (*prescaler <= 1024))
      {
        /* store the bustiming configuration */
        *tseg1 = canTiming[cnt].tseg1;
        *tseg2 = canTiming[cnt].tseg2;
        /* found a good bus timing configuration */
        return BLT_TRUE;
      }
    }
  }
  /* could not find a good bus timing configuration */
  return BLT_FALSE;
} /*** end of CanGetSpeedConfig ***/


/************************************************************************************//**
** \brief     Initializes the CAN controller and synchronizes it to the CAN bus.
** \return    none.
**
****************************************************************************************/
void CanInit(void)
{
  blt_int16u prescaler = 0;
  blt_int8u  tseg1 = 0, tseg2 = 0;
  CAN_FilterTypeDef filterConfig;
  blt_int32u rxMsgId;
  blt_int32u rxFilterId, rxFilterMask;

  /* dingoFW: pull the baudrate and XCP message ids from the application's config sector
   * (falls back to the blt_conf.h defaults if it is blank/invalid). */
  DingoLoadCanConfig();
  rxMsgId = dingoRxMsgId;

  /* the current implementation supports CAN1. throw an assertion error in case a
   * different CAN channel is configured.
   */
  ASSERT_CT(BOOT_COM_CAN_CHANNEL_INDEX == 0);
  /* obtain bittiming configuration information. */
  if (CanGetSpeedConfig(dingoBaudrate/1000, &prescaler, &tseg1, &tseg2) == BLT_FALSE)
  {
    /* Incorrect configuration. The specified baudrate is not supported for the given
     * clock configuration. Verify the following settings in blt_conf.h:
     *   - BOOT_COM_CAN_BAUDRATE
     *   - BOOT_CPU_XTAL_SPEED_KHZ
     *   - BOOT_CPU_SYSTEM_SPEED_KHZ
     */
    ASSERT_RT(BLT_FALSE);
  }

  /* set the CAN controller configuration. */
  canHandle.Instance = CAN_CHANNEL;
  canHandle.Init.TimeTriggeredMode = DISABLE;
  canHandle.Init.AutoBusOff = DISABLE;
  canHandle.Init.AutoWakeUp = DISABLE;
  canHandle.Init.AutoRetransmission = ENABLE;
  canHandle.Init.ReceiveFifoLocked = DISABLE;
  canHandle.Init.TransmitFifoPriority = DISABLE;
  canHandle.Init.Mode = CAN_MODE_NORMAL;
  canHandle.Init.SyncJumpWidth = CAN_SJW_1TQ;
  canHandle.Init.TimeSeg1 = ((blt_int32u)tseg1 - 1) << CAN_BTR_TS1_Pos;
  canHandle.Init.TimeSeg2 = ((blt_int32u)tseg2 - 1) << CAN_BTR_TS2_Pos;
  canHandle.Init.Prescaler = prescaler;
  /* initialize the CAN controller. this only fails if the CAN controller hardware is
   * faulty. no need to evaluate the return value as there is nothing we can do about
   * a faulty CAN controller.
   */
  (void)HAL_CAN_Init(&canHandle);
  /* determine the reception filter mask and id values such that it only leaves one
   * CAN identifier through (BOOT_COM_CAN_RX_MSG_ID).
   */
  if ((rxMsgId & 0x80000000) == 0)
  {
    rxFilterId = rxMsgId << CAN_RI0R_STID_Pos;
    rxFilterMask = (CAN_RI0R_STID_Msk) | CAN_RI0R_IDE;
  }
  else
  {
    /* negate the ID-type bit */
    rxMsgId &= ~0x80000000;
    rxFilterId = (rxMsgId << CAN_RI0R_EXID_Pos) | CAN_RI0R_IDE;
    rxFilterMask = (CAN_RI0R_EXID_Msk) | CAN_RI0R_IDE;
  }
  /* configure the reception filter. note that the implementation of this function
   * always returns HAL_OK, so no need to evaluate the return value.
   */
  /* filter 0 is the first filter assigned to the bxCAN master (CAN1) */
  filterConfig.FilterBank = 0;
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  filterConfig.FilterIdHigh = (rxFilterId >> 16) & 0x0000FFFFu;
  filterConfig.FilterIdLow = rxFilterId & 0x0000FFFFu;
  filterConfig.FilterMaskIdHigh = (rxFilterMask >> 16) & 0x0000FFFFu;
  filterConfig.FilterMaskIdLow = rxFilterMask & 0x0000FFFFu;
  filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  filterConfig.FilterActivation = ENABLE;
  /* select the start slave bank number (for CAN1). this configuration assigns filter
   * banks 0..13 to CAN1 and 14..27 to CAN2.
   */
  filterConfig.SlaveStartFilterBank = 14;
  (void)HAL_CAN_ConfigFilter(&canHandle, &filterConfig);
  /* start the CAN peripheral. no need to evaluate the return value as there is nothing
   * we can do about a faulty CAN controller. */
  (void)HAL_CAN_Start(&canHandle);
} /*** end of CanInit ***/


/************************************************************************************//**
** \brief     Transmits a packet formatted for the communication interface.
** \param     data Pointer to byte array with data that it to be transmitted.
** \param     len  Number of bytes that are to be transmitted.
** \return    none.
**
****************************************************************************************/
void CanTransmitPacket(blt_int8u *data, blt_int8u len)
{
  blt_int32u txMsgId = dingoTxMsgId;
  CAN_TxHeaderTypeDef txMsgHeader;
  blt_int32u txMsgMailbox;
  blt_int32u timeout;
  HAL_StatusTypeDef txStatus;

  /* configure the message that should be transmitted. */
  if ((txMsgId & 0x80000000) == 0)
  {
    /* set the 11-bit CAN identifier. */
    txMsgHeader.StdId = txMsgId;
    txMsgHeader.IDE = CAN_ID_STD;
  }
  else
  {
    /* negate the ID-type bit */
    txMsgId &= ~0x80000000;
    /* set the 29-bit CAN identifier. */
    txMsgHeader.ExtId = txMsgId;
    txMsgHeader.IDE = CAN_ID_EXT;
  }
  txMsgHeader.RTR = CAN_RTR_DATA;
  txMsgHeader.DLC = len;

  /* submit the message for transmission. */
  txStatus = HAL_CAN_AddTxMessage(&canHandle, &txMsgHeader, data,
                                  (uint32_t *)&txMsgMailbox);
  if (txStatus == HAL_OK)
  {
    /* determine timeout time for the transmit completion. */
    timeout = TimerGet() + CAN_MSG_TX_TIMEOUT_MS;
    /* poll for completion of the transmit operation. */
    while (HAL_CAN_IsTxMessagePending(&canHandle, txMsgMailbox) != 0)
    {
      /* service the watchdog. */
      CopService();
      /* break loop upon timeout. this would indicate a hardware failure or no other
       * nodes connected to the bus.
       */
      if (TimerGet() > timeout)
      {
        break;
      }
    }
  }
} /*** end of CanTransmitPacket ***/


/************************************************************************************//**
** \brief     Receives a communication interface packet if one is present.
** \param     data Pointer to byte array where the data is to be stored.
** \param     len Pointer where the length of the packet is to be stored.
** \return    BLT_TRUE is a packet was received, BLT_FALSE otherwise.
**
****************************************************************************************/
blt_bool CanReceivePacket(blt_int8u *data, blt_int8u *len)
{
  blt_int32u rxMsgId = dingoRxMsgId;
  blt_bool result = BLT_FALSE;
  CAN_RxHeaderTypeDef rxMsgHeader;

  if (HAL_CAN_GetRxMessage(&canHandle, CAN_RX_FIFO0, &rxMsgHeader, data) == HAL_OK)
  {
    /* check if this message has the configured CAN packet identifier. */
    if ((rxMsgId & 0x80000000) == 0)
    {
      /* was an 11-bit CAN message received that matches? */
      if ( (rxMsgHeader.StdId == rxMsgId) &&
           (rxMsgHeader.IDE == CAN_ID_STD) )
      {
        /* set flag that a packet with a matching CAN identifier was received. */
        result = BLT_TRUE;
      }
    }
    else
    {
      /* negate the ID-type bit. */
      rxMsgId &= ~0x80000000;
      /* was an 29-bit CAN message received that matches? */
      if ( (rxMsgHeader.ExtId == rxMsgId) &&
           (rxMsgHeader.IDE == CAN_ID_EXT) )
      {
        /* set flag that a packet with a matching CAN identifier was received. */
        result = BLT_TRUE;
      }
    }
  }
  /* store the data length. */
  if (result == BLT_TRUE)
  {
    *len = rxMsgHeader.DLC;
  }
  /* Give the result back to the caller. */
  return result;
} /*** end of CanReceivePacket ***/
#endif /* BOOT_COM_CAN_ENABLE > 0 */


/*********************************** end of can.c **************************************/

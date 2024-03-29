/*******************************************************************************
  CANFD Peripheral Library Interface Source File

  Company:
    Microchip Technology Inc.

  File Name:
    plib_canfd2.c

  Summary:
    CANFD peripheral library interface.

  Description:
    This file defines the interface to the CANFD peripheral library. This
    library provides access to and control of the associated peripheral
    instance.

  Remarks:
    None.
*******************************************************************************/

//DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
//DOM-IGNORE-END
// *****************************************************************************
// *****************************************************************************
// Header Includes
// *****************************************************************************
// *****************************************************************************
#include <sys/kmem.h>
#include "plib_canfd2.h"


// *****************************************************************************
// *****************************************************************************
// Global Data
// *****************************************************************************
// *****************************************************************************
/* CAN2 Message memory size */
#define CANFD_MESSAGE_RAM_CONFIG_SIZE 448
/* Number of configured FIFO */
#define CANFD_NUM_OF_FIFO             2
/* Maximum number of CAN Message buffers in each FIFO */
#define CANFD_FIFO_MESSAGE_BUFFER_MAX 32

#define CANFD_CONFIGURATION_MODE      0x4
#define CANFD_OPERATION_MODE          0x0
#define CANFD_NUM_OF_FILTER           1
/* FIFO Offset in word (4 bytes) */
#define CANFD_FIFO_OFFSET             0xc
/* Filter Offset in word (4 bytes) */
#define CANFD_FILTER_OFFSET           0x4
#define CANFD_FILTER_OBJ_OFFSET       0x8
/* Acceptance Mask Offset in word (4 bytes) */
#define CANFD_ACCEPTANCE_MASK_OFFSET  0x8
#define CANFD_MSG_SID_MASK            0x7FF
#define CANFD_MSG_EID_MASK            0x1FFFFFFF
#define CANFD_MSG_DLC_MASK            0x0000000F
#define CANFD_MSG_IDE_MASK            0x00000010
#define CANFD_MSG_RTR_MASK            0x00000020
#define CANFD_MSG_BRS_MASK            0x00000040
#define CANFD_MSG_FDF_MASK            0x00000080
#define CANFD_MSG_SEQ_MASK            0xFFFFFE00
#define CANFD_MSG_TX_EXT_SID_MASK     0x1FFC0000
#define CANFD_MSG_TX_EXT_EID_MASK     0x0003FFFF
#define CANFD_MSG_RX_EXT_SID_MASK     0x000007FF
#define CANFD_MSG_RX_EXT_EID_MASK     0x1FFFF800
#define CANFD_MSG_FLT_EXT_SID_MASK    0x1FFC0000
#define CANFD_MSG_FLT_EXT_EID_MASK    0x0003FFFF

static CANFD_OBJ can2Obj;
static CANFD_RX_MSG can2RxMsg[CANFD_NUM_OF_FIFO][CANFD_FIFO_MESSAGE_BUFFER_MAX];
static CANFD_CALLBACK_OBJ can2CallbackObj[CANFD_NUM_OF_FIFO + 1];
static CANFD_CALLBACK_OBJ can2ErrorCallbackObj;
static uint32_t can2MsgIndex[CANFD_NUM_OF_FIFO];
static uint8_t __attribute__((coherent, aligned(16))) can_message_buffer[CANFD_MESSAGE_RAM_CONFIG_SIZE];
static const uint8_t dlcToLength[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};

/******************************************************************************
Local Functions
******************************************************************************/
static void CANLengthToDlcGet(uint8_t length, uint8_t *dlc)
{
    if (length <= 8)
    {
        *dlc = length;
    }
    else if (length <= 12)
    {
        *dlc = 0x9;
    }
    else if (length <= 16)
    {
        *dlc = 0xA;
    }
    else if (length <= 20)
    {
        *dlc = 0xB;
    }
    else if (length <= 24)
    {
        *dlc = 0xC;
    }
    else if (length <= 32)
    {
        *dlc = 0xD;
    }
    else if (length <= 48)
    {
        *dlc = 0xE;
    }
    else
    {
        *dlc = 0xF;
    }
}

// *****************************************************************************
// *****************************************************************************
// CAN2 PLib Interface Routines
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************
/* Function:
    void CAN2_Initialize(void)

   Summary:
    Initializes given instance of the CAN peripheral.

   Precondition:
    None.

   Parameters:
    None.

   Returns:
    None
*/
void CAN2_Initialize(void)
{
    /* Switch the CAN module ON */
    CFD2CON |= _CFD2CON_ON_MASK;

    /* Switch the CAN module to Configuration mode. Wait until the switch is complete */
    CFD2CON = (CFD2CON & ~_CFD2CON_REQOP_MASK) | ((CANFD_CONFIGURATION_MODE << _CFD2CON_REQOP_POSITION) & _CFD2CON_REQOP_MASK);
    while(((CFD2CON & _CFD2CON_OPMOD_MASK) >> _CFD2CON_OPMOD_POSITION) != CANFD_CONFIGURATION_MODE);

    /* Set the Data bitrate to 8000 Kbps */
    CFD2DBTCFG = ((0 << _CFD2DBTCFG_BRP_POSITION) & _CFD2DBTCFG_BRP_MASK)
               | ((12 << _CFD2DBTCFG_TSEG1_POSITION) & _CFD2DBTCFG_TSEG1_MASK)
               | ((0 << _CFD2DBTCFG_TSEG2_POSITION) & _CFD2DBTCFG_TSEG2_MASK)
               | ((0 << _CFD2DBTCFG_SJW_POSITION) & _CFD2DBTCFG_SJW_MASK);

    /* Set the Nominal bitrate to 1000 Kbps */
    CFD2NBTCFG = ((0 << _CFD2NBTCFG_BRP_POSITION) & _CFD2NBTCFG_BRP_MASK)
               | ((117 << _CFD2NBTCFG_TSEG1_POSITION) & _CFD2NBTCFG_TSEG1_MASK)
               | ((0 << _CFD2NBTCFG_TSEG2_POSITION) & _CFD2NBTCFG_TSEG2_MASK)
               | ((0 << _CFD2NBTCFG_SJW_POSITION) & _CFD2NBTCFG_SJW_MASK);

    /* Set Message memory base address for all FIFOs/Queue */
    CFD2FIFOBA = (uint32_t)KVA_TO_PA(can_message_buffer);

    /* Tx Event FIFO Configuration */
    CFD2TEFCON = (((2 - 1) << _CFD2TEFCON_FSIZE_POSITION) & _CFD2TEFCON_FSIZE_MASK);
    CFD2CON |= _CFD2CON_STEF_MASK;

    /* Tx Queue Configuration */
    CFD2TXQCON = (((2 - 1) << _CFD2TXQCON_FSIZE_POSITION) & _CFD2TXQCON_FSIZE_MASK)
               | ((0x7 << _CFD2TXQCON_PLSIZE_POSITION) & _CFD2TXQCON_PLSIZE_MASK)
               | ((0x0 << _CFD2TXQCON_TXPRI_POSITION) & _CFD2TXQCON_TXPRI_MASK);
    CFD2CON |= _CFD2CON_TXQEN_MASK;


    /* Configure CAN FIFOs */
    CFD2FIFOCON1 = (((2 - 1) << _CFD2FIFOCON1_FSIZE_POSITION) & _CFD2FIFOCON1_FSIZE_MASK) | _CFD2FIFOCON1_TXEN_MASK | ((0x0 << _CFD2FIFOCON1_TXPRI_POSITION) & _CFD2FIFOCON1_TXPRI_MASK) | ((0x0 << _CFD2FIFOCON1_RTREN_POSITION) & _CFD2FIFOCON1_RTREN_MASK) | ((0x7 << _CFD2FIFOCON1_PLSIZE_POSITION) & _CFD2FIFOCON1_PLSIZE_MASK);
    CFD2FIFOCON2 = (((2 - 1) << _CFD2FIFOCON2_FSIZE_POSITION) & _CFD2FIFOCON2_FSIZE_MASK) | ((0x7 << _CFD2FIFOCON2_PLSIZE_POSITION) & _CFD2FIFOCON2_PLSIZE_MASK);

    /* Configure CAN Filters */
    /* Filter 0 configuration */
    CFD2FLTOBJ0 = (0 & CANFD_MSG_SID_MASK);
    CFD2MASK0 = (0 & CANFD_MSG_SID_MASK);
    CFD2FLTCON0 |= (((0x2 << _CFD2FLTCON0_F0BP_POSITION) & _CFD2FLTCON0_F0BP_MASK)| _CFD2FLTCON0_FLTEN0_MASK);

    /* Set Interrupts */
    IEC5SET = _IEC5_CAN2IE_MASK;
    CFD2INT |= _CFD2INT_SERRIE_MASK | _CFD2INT_CERRIE_MASK | _CFD2INT_IVMIE_MASK;

    /* Initialize the CAN PLib Object */
    memset((void *)can2RxMsg, 0x00, sizeof(can2RxMsg));

    /* Switch the CAN module to CANFD_OPERATION_MODE. Wait until the switch is complete */
    CFD2CON = (CFD2CON & ~_CFD2CON_REQOP_MASK) | ((CANFD_OPERATION_MODE << _CFD2CON_REQOP_POSITION) & _CFD2CON_REQOP_MASK);
    while(((CFD2CON & _CFD2CON_OPMOD_MASK) >> _CFD2CON_OPMOD_POSITION) != CANFD_OPERATION_MODE);
}

// *****************************************************************************
/* Function:
    bool CAN2_MessageTransmit(uint32_t id, uint8_t length, uint8_t* data, uint8_t fifoQueueNum, CANFD_MODE mode, CANFD_MSG_TX_ATTRIBUTE msgAttr)

   Summary:
    Transmits a message into CAN bus.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    id           - 11-bit / 29-bit identifier (ID).
    length       - Length of data buffer in number of bytes.
    data         - Pointer to source data buffer
    fifoQueueNum - If fifoQueueNum is 0 then Transmit Queue otherwise FIFO
    mode         - CAN mode Classic CAN or CAN FD without BRS or CAN FD with BRS
    msgAttr      - Data frame or Remote frame to be transmitted

   Returns:
    Request status.
    true  - Request was successful.
    false - Request has failed.
*/
bool CAN2_MessageTransmit(uint32_t id, uint8_t length, uint8_t* data, uint8_t fifoQueueNum, CANFD_MODE mode, CANFD_MSG_TX_ATTRIBUTE msgAttr)
{
    CANFD_TX_MSG_OBJECT *txMessage = NULL;
    static uint32_t sequence = 0;
    uint8_t count = 0;
    uint8_t dlc = 0;
    bool status = false;

    if (fifoQueueNum == 0)
    {
        if ((CFD2TXQSTA & _CFD2TXQSTA_TXQNIF_MASK) == _CFD2TXQSTA_TXQNIF_MASK)
        {
            txMessage = (CANFD_TX_MSG_OBJECT *)PA_TO_KVA1(CFD2TXQUA);
            status = true;
        }
    }
    else if (fifoQueueNum <= CANFD_NUM_OF_FIFO)
    {
        if ((*(volatile uint32_t *)(&CFD2FIFOSTA1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOSTA1_TFNRFNIF_MASK) == _CFD2FIFOSTA1_TFNRFNIF_MASK)
        {
            txMessage = (CANFD_TX_MSG_OBJECT *)PA_TO_KVA1(*(volatile uint32_t *)(&CFD2FIFOUA1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)));
            status = true;
        }
    }

    if (status)
    {
        /* Check the id whether it falls under SID or EID,
         * SID max limit is 0x7FF, so anything beyond that is EID */
        if (id > CANFD_MSG_SID_MASK)
        {
            txMessage->t0 = (((id & CANFD_MSG_TX_EXT_SID_MASK) >> 18) | ((id & CANFD_MSG_TX_EXT_EID_MASK) << 11)) & CANFD_MSG_EID_MASK;
            txMessage->t1 = CANFD_MSG_IDE_MASK;
        }
        else
        {
            txMessage->t0 = id;
            txMessage->t1 = 0;
        }
        if (length > 64)
            length = 64;

        CANLengthToDlcGet(length, &dlc);

        txMessage->t1 |= (dlc & CANFD_MSG_DLC_MASK);

        if(mode == CANFD_MODE_FD_WITH_BRS)
        {
            txMessage->t1 |= CANFD_MSG_FDF_MASK | CANFD_MSG_BRS_MASK;
        }
        else if (mode == CANFD_MODE_FD_WITHOUT_BRS)
        {
            txMessage->t1 |= CANFD_MSG_FDF_MASK;
        }
        if (msgAttr == CANFD_MSG_TX_REMOTE_FRAME)
        {
            txMessage->t1 |= CANFD_MSG_RTR_MASK;
        }
        else
        {
            while(count < length)
            {
                txMessage->data[count++] = *data++;
            }
        }

        txMessage->t1 |= ((++sequence << 9) & CANFD_MSG_SEQ_MASK);

        if (fifoQueueNum == 0)
        {
            CFD2TXQCON |= _CFD2TXQCON_TXQEIE_MASK;

            /* Request the transmit */
            CFD2TXQCON |= _CFD2TXQCON_UINC_MASK;
            CFD2TXQCON |= _CFD2TXQCON_TXREQ_MASK;
        }
        else
        {
            *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_TFERFFIE_MASK;

            /* Request the transmit */
            *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_UINC_MASK;
            *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_TXREQ_MASK;
        }
        CFD2INT |= _CFD2INT_TXIE_MASK;
    }
    return status;
}

// *****************************************************************************
/* Function:
    bool CAN2_MessageReceive(uint32_t *id, uint8_t *length, uint8_t *data, uint32_t *timestamp, uint8_t fifoNum, CANFD_MSG_RX_ATTRIBUTE *msgAttr)

   Summary:
    Receives a message from CAN bus.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    id          - Pointer to 11-bit / 29-bit identifier (ID) to be received.
    length      - Pointer to data length in number of bytes to be received.
    data        - Pointer to destination data buffer
    timestamp   - Pointer to Rx message timestamp, timestamp value is 0 if Timestamp is disabled in CFD2TSCON
    fifoNum     - FIFO number
    msgAttr     - Data frame or Remote frame to be received

   Returns:
    Request status.
    true  - Request was successful.
    false - Request has failed.
*/
bool CAN2_MessageReceive(uint32_t *id, uint8_t *length, uint8_t *data, uint32_t *timestamp, uint8_t fifoNum, CANFD_MSG_RX_ATTRIBUTE *msgAttr)
{
    bool status = false;
    uint8_t msgIndex = 0;
    uint8_t fifoSize = 0;

    if ((fifoNum > CANFD_NUM_OF_FIFO) || (id == NULL))
    {
        return status;
    }

    fifoSize = (*(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOCON1_FSIZE_MASK) >> _CFD2FIFOCON1_FSIZE_POSITION;
    for (msgIndex = 0; msgIndex <= fifoSize; msgIndex++)
    {
        if ((can2MsgIndex[fifoNum-1] & (1UL << (msgIndex & 0x1F))) == 0)
        {
            can2MsgIndex[fifoNum-1] |= (1UL << (msgIndex & 0x1F));
            break;
        }
    }
    if(msgIndex > fifoSize)
    {
        /* FIFO is full */
        return false;
    }
    can2RxMsg[fifoNum-1][msgIndex].id = id;
    can2RxMsg[fifoNum-1][msgIndex].buffer = data;
    can2RxMsg[fifoNum-1][msgIndex].size = length;
    can2RxMsg[fifoNum-1][msgIndex].timestamp = timestamp;
    can2RxMsg[fifoNum-1][msgIndex].msgAttr = msgAttr;
    *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_TFNRFNIE_MASK;
    CFD2INT |= _CFD2INT_RXIE_MASK;
    status = true;

    return status;
}

// *****************************************************************************
/* Function:
    void CAN2_MessageAbort(uint8_t fifoQueueNum)

   Summary:
    Abort request for a Queue/FIFO.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    fifoQueueNum - If fifoQueueNum is 0 then Transmit Queue otherwise FIFO

   Returns:
    None.
*/
void CAN2_MessageAbort(uint8_t fifoQueueNum)
{
    if (fifoQueueNum == 0)
    {
        CFD2TXQCON &= ~_CFD2TXQCON_TXREQ_MASK;
    }
    else if (fifoQueueNum <= CANFD_NUM_OF_FIFO)
    {
        *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) &= ~_CFD2FIFOCON1_TXREQ_MASK;
    }
}

// *****************************************************************************
/* Function:
    void CAN2_MessageAcceptanceFilterSet(uint8_t filterNum, uint32_t id)

   Summary:
    Set Message acceptance filter configuration.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    filterNum - Filter number
    id        - 11-bit or 29-bit identifier

   Returns:
    None.
*/
void CAN2_MessageAcceptanceFilterSet(uint8_t filterNum, uint32_t id)
{
    uint32_t filterEnableBit = 0;
    uint8_t filterRegIndex = 0;

    if (filterNum < CANFD_NUM_OF_FILTER)
    {
        filterRegIndex = filterNum >> 2;
        filterEnableBit = (filterNum % 4 == 0)? _CFD2FLTCON0_FLTEN0_MASK : 1 << ((((filterNum % 4) + 1) * 8) - 1);

        *(volatile uint32_t *)(&CFD2FLTCON0 + (filterRegIndex * CANFD_FILTER_OFFSET)) &= ~filterEnableBit;

        if (id > CANFD_MSG_SID_MASK)
        {
            *(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) = ((((id & CANFD_MSG_FLT_EXT_SID_MASK) >> 18) | ((id & CANFD_MSG_FLT_EXT_EID_MASK) << 11)) & CANFD_MSG_EID_MASK) | _CFD2FLTOBJ0_EXIDE_MASK;
        }
        else
        {
            *(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) = id & CANFD_MSG_SID_MASK;
        }
        *(volatile uint32_t *)(&CFD2FLTCON0 + (filterRegIndex * CANFD_FILTER_OFFSET)) |= filterEnableBit;
    }
}

// *****************************************************************************
/* Function:
    uint32_t CAN2_MessageAcceptanceFilterGet(uint8_t filterNum)

   Summary:
    Get Message acceptance filter configuration.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    filterNum - Filter number

   Returns:
    Returns Message acceptance filter identifier
*/
uint32_t CAN2_MessageAcceptanceFilterGet(uint8_t filterNum)
{
    uint32_t id = 0;

    if (filterNum < CANFD_NUM_OF_FILTER)
    {
        if (*(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) & _CFD2FLTOBJ0_EXIDE_MASK)
        {
            id = (((*(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) & CANFD_MSG_RX_EXT_SID_MASK) << 18)
               | ((*(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) & CANFD_MSG_RX_EXT_EID_MASK) >> 11))
               & CANFD_MSG_EID_MASK;
        }
        else
        {
            id = (*(volatile uint32_t *)(&CFD2FLTOBJ0 + (filterNum * CANFD_FILTER_OBJ_OFFSET)) & CANFD_MSG_SID_MASK);
        }
    }
    return id;
}

// *****************************************************************************
/* Function:
    void CAN2_MessageAcceptanceFilterMaskSet(uint8_t acceptanceFilterMaskNum, uint32_t id)

   Summary:
    Set Message acceptance filter mask configuration.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    acceptanceFilterMaskNum - Acceptance Filter Mask number
    id                      - 11-bit or 29-bit identifier

   Returns:
    None.
*/
void CAN2_MessageAcceptanceFilterMaskSet(uint8_t acceptanceFilterMaskNum, uint32_t id)
{
    /* Switch the CAN module to Configuration mode. Wait until the switch is complete */
    CFD2CON = (CFD2CON & ~_CFD2CON_REQOP_MASK) | ((CANFD_CONFIGURATION_MODE << _CFD2CON_REQOP_POSITION) & _CFD2CON_REQOP_MASK);
    while(((CFD2CON & _CFD2CON_OPMOD_MASK) >> _CFD2CON_OPMOD_POSITION) != CANFD_CONFIGURATION_MODE);

    if (id > CANFD_MSG_SID_MASK)
    {
        *(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) = ((((id & CANFD_MSG_FLT_EXT_SID_MASK) >> 18) | ((id & CANFD_MSG_FLT_EXT_EID_MASK) << 11)) & CANFD_MSG_EID_MASK) | _CFD2MASK0_MIDE_MASK;
    }
    else
    {
        *(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) = id & CANFD_MSG_SID_MASK;
    }

    /* Switch the CAN module to CANFD_OPERATION_MODE. Wait until the switch is complete */
    CFD2CON = (CFD2CON & ~_CFD2CON_REQOP_MASK) | ((CANFD_OPERATION_MODE << _CFD2CON_REQOP_POSITION) & _CFD2CON_REQOP_MASK);
    while(((CFD2CON & _CFD2CON_OPMOD_MASK) >> _CFD2CON_OPMOD_POSITION) != CANFD_OPERATION_MODE);
}

// *****************************************************************************
/* Function:
    uint32_t CAN2_MessageAcceptanceFilterMaskGet(uint8_t acceptanceFilterMaskNum)

   Summary:
    Get Message acceptance filter mask configuration.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    acceptanceFilterMaskNum - Acceptance Filter Mask number

   Returns:
    Returns Message acceptance filter mask.
*/
uint32_t CAN2_MessageAcceptanceFilterMaskGet(uint8_t acceptanceFilterMaskNum)
{
    uint32_t id = 0;

    if (*(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) & _CFD2MASK0_MIDE_MASK)
    {
        id = (((*(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) & CANFD_MSG_RX_EXT_SID_MASK) << 18)
           | ((*(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) & CANFD_MSG_RX_EXT_EID_MASK) >> 11))
           & CANFD_MSG_EID_MASK;
    }
    else
    {
        id = (*(volatile uint32_t *)(&CFD2MASK0 + (acceptanceFilterMaskNum * CANFD_ACCEPTANCE_MASK_OFFSET)) & CANFD_MSG_SID_MASK);
    }
    return id;
}

// *****************************************************************************
/* Function:
    bool CAN2_TransmitEventFIFOElementGet(uint32_t *id, uint32_t *sequence, uint32_t *timestamp)

   Summary:
    Get the Transmit Event FIFO Element for the transmitted message.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    id          - Pointer to 11-bit / 29-bit identifier (ID) to be received.
    sequence    - Pointer to Tx message sequence number to be received
    timestamp   - Pointer to Tx message timestamp to be received, timestamp value is 0 if Timestamp is disabled in CFD2TSCON

   Returns:
    Request status.
    true  - Request was successful.
    false - Request has failed.
*/
bool CAN2_TransmitEventFIFOElementGet(uint32_t *id, uint32_t *sequence, uint32_t *timestamp)
{
    CANFD_TX_EVENT_FIFO_ELEMENT *txEventFIFOElement = NULL;
    bool status = false;

    /* Check if there is a message available in Tx Event FIFO */
    if ((CFD2TEFSTA & _CFD2TEFSTA_TEFNEIF_MASK) == _CFD2TEFSTA_TEFNEIF_MASK)
    {
        /* Get a pointer to Tx Event FIFO Element */
        txEventFIFOElement = (CANFD_TX_EVENT_FIFO_ELEMENT *)PA_TO_KVA1(CFD2TEFUA);

        /* Check if it's a extended message type */
        if (txEventFIFOElement->te1 & CANFD_MSG_IDE_MASK)
        {
            *id = txEventFIFOElement->te0 & CANFD_MSG_EID_MASK;
        }
        else
        {
            *id = txEventFIFOElement->te0 & CANFD_MSG_SID_MASK;
        }

        *sequence = ((txEventFIFOElement->te1 & CANFD_MSG_SEQ_MASK) >> 9);

        if (timestamp != NULL)
        {
        }

        /* Tx Event FIFO Element read done, update the Tx Event FIFO tail */
        CFD2TEFCON |= _CFD2TEFCON_UINC_MASK;

        /* Tx Event FIFO Element read successfully, so return true */
        status = true;
    }
    return status;
}

// *****************************************************************************
/* Function:
    CANFD_ERROR CAN2_ErrorGet(void)

   Summary:
    Returns the error during transfer.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    None.

   Returns:
    Error during transfer.
*/
CANFD_ERROR CAN2_ErrorGet(void)
{
    return (CANFD_ERROR)can2Obj.errorStatus;
}

// *****************************************************************************
/* Function:
    void CAN2_ErrorCountGet(uint8_t *txErrorCount, uint8_t *rxErrorCount)

   Summary:
    Returns the transmit and receive error count during transfer.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    txErrorCount - Transmit Error Count to be received
    rxErrorCount - Receive Error Count to be received

   Returns:
    None.
*/
void CAN2_ErrorCountGet(uint8_t *txErrorCount, uint8_t *rxErrorCount)
{
    *txErrorCount = (uint8_t)((CFD2TREC & _CFD2TREC_TERRCNT_MASK) >> _CFD2TREC_TERRCNT_POSITION);
    *rxErrorCount = (uint8_t)(CFD2TREC & _CFD2TREC_RERRCNT_MASK);
}

// *****************************************************************************
/* Function:
    bool CAN2_InterruptGet(uint8_t fifoQueueNum, CANFD_FIFO_INTERRUPT_FLAG_MASK fifoInterruptFlagMask)

   Summary:
    Returns the FIFO Interrupt status.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    fifoQueueNum          - FIFO number
    fifoInterruptFlagMask - FIFO interrupt flag mask

   Returns:
    true - Requested fifo interrupt is occurred.
    false - Requested fifo interrupt is not occurred.
*/
bool CAN2_InterruptGet(uint8_t fifoQueueNum, CANFD_FIFO_INTERRUPT_FLAG_MASK fifoInterruptFlagMask)
{
    if (fifoQueueNum == 0)
    {
        return ((CFD2TXQSTA & fifoInterruptFlagMask) != 0x0);
    }
    else
    {
        return ((*(volatile uint32_t *)(&CFD2FIFOSTA1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) & fifoInterruptFlagMask) != 0x0);
    }
}

// *****************************************************************************
/* Function:
    bool CAN2_TxFIFOQueueIsFull(uint8_t fifoQueueNum)

   Summary:
    Returns true if Tx FIFO/Queue is full otherwise false.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    fifoQueueNum - FIFO/Queue number

   Returns:
    true  - Tx FIFO/Queue is full.
    false - Tx FIFO/Queue is not full.
*/
bool CAN2_TxFIFOQueueIsFull(uint8_t fifoQueueNum)
{
    if (fifoQueueNum == 0)
    {
        return ((CFD2TXQSTA & _CFD2TXQSTA_TXQNIF_MASK) != _CFD2TXQSTA_TXQNIF_MASK);
    }
    else
    {
        return ((*(volatile uint32_t *)(&CFD2FIFOSTA1 + ((fifoQueueNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOSTA1_TFNRFNIF_MASK) != _CFD2FIFOSTA1_TFNRFNIF_MASK);
    }
}

// *****************************************************************************
/* Function:
    bool CAN2_AutoRTRResponseSet(uint32_t id, uint8_t length, uint8_t* data, uint8_t fifoNum)

   Summary:
    Set the Auto RTR response for remote transmit request.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.
    Auto RTR Enable must be set to 0x1 for the requested Transmit FIFO in MHC configuration.

   Parameters:
    id           - 11-bit / 29-bit identifier (ID).
    length       - Length of data buffer in number of bytes.
    data         - Pointer to source data buffer
    fifoNum      - FIFO Number

   Returns:
    Request status.
    true  - Request was successful.
    false - Request has failed.
*/
bool CAN2_AutoRTRResponseSet(uint32_t id, uint8_t length, uint8_t* data, uint8_t fifoNum)
{
    CANFD_TX_MSG_OBJECT *txMessage = NULL;
    uint8_t count = 0;
    bool status = false;

    if (fifoNum <= CANFD_NUM_OF_FIFO)
    {
        if ((*(volatile uint32_t *)(&CFD2FIFOSTA1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOSTA1_TFNRFNIF_MASK) == _CFD2FIFOSTA1_TFNRFNIF_MASK)
        {
            txMessage = (CANFD_TX_MSG_OBJECT *)PA_TO_KVA1(*(volatile uint32_t *)(&CFD2FIFOUA1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)));
            status = true;
        }
    }

    if (status)
    {
        /* Check the id whether it falls under SID or EID,
         * SID max limit is 0x7FF, so anything beyond that is EID */
        if (id > CANFD_MSG_SID_MASK)
        {
            txMessage->t0 = (((id & CANFD_MSG_TX_EXT_SID_MASK) >> 18) | ((id & CANFD_MSG_TX_EXT_EID_MASK) << 11)) & CANFD_MSG_EID_MASK;
            txMessage->t1 = CANFD_MSG_IDE_MASK;
        }
        else
        {
            txMessage->t0 = id;
            txMessage->t1 = 0;
        }

        /* Limit length */
        if (length > 8)
            length = 8;
        txMessage->t1 |= length;

        while(count < length)
        {
            txMessage->data[count++] = *data++;
        }

        *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_TFERFFIE_MASK;

        /* Set UINC to respond to RTR */
        *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_UINC_MASK;
        CFD2INT |= _CFD2INT_TXIE_MASK;
    }
    return status;
}

// *****************************************************************************
/* Function:
    void CAN2_CallbackRegister(CANFD_CALLBACK callback, uintptr_t contextHandle, uint8_t fifoQueueNum)

   Summary:
    Sets the pointer to the function (and it's context) to be called when the
    given CAN's transfer events occur.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    callback - A pointer to a function with a calling signature defined
    by the CANFD_CALLBACK data type.
    fifoQueueNum - Tx Queue or Tx/Rx FIFO number

    context - A value (usually a pointer) passed (unused) into the function
    identified by the callback parameter.

   Returns:
    None.
*/
void CAN2_CallbackRegister(CANFD_CALLBACK callback, uintptr_t contextHandle, uint8_t fifoQueueNum)
{
    if (callback == NULL)
    {
        return;
    }

    can2CallbackObj[fifoQueueNum].callback = callback;
    can2CallbackObj[fifoQueueNum].context = contextHandle;
}

// *****************************************************************************
/* Function:
    void CAN2_ErrorCallbackRegister(CANFD_CALLBACK callback, uintptr_t contextHandle)

   Summary:
    Sets the pointer to the function (and it's context) to be called when the
    given CAN's transfer events occur.

   Precondition:
    CAN2_Initialize must have been called for the associated CAN instance.

   Parameters:
    callback - A pointer to a function with a calling signature defined
    by the CANFD_CALLBACK data type.

    context - A value (usually a pointer) passed (unused) into the function
    identified by the callback parameter.

   Returns:
    None.
*/
void CAN2_ErrorCallbackRegister(CANFD_CALLBACK callback, uintptr_t contextHandle)
{
    if (callback == NULL)
    {
        return;
    }

    can2ErrorCallbackObj.callback = callback;
    can2ErrorCallbackObj.context = contextHandle;
}

static void CAN2_RX_InterruptHandler(void)
{
    uint8_t  msgIndex = 0;
    uint8_t  fifoNum = 0;
    uint8_t  fifoSize = 0;
    uint8_t  count = 0;
    CANFD_RX_MSG_OBJECT *rxMessage = NULL;

    fifoNum = (uint8_t)CFD2VEC & _CFD2VEC_ICODE_MASK;
    if (fifoNum <= CANFD_NUM_OF_FIFO)
    {
        fifoSize = (*(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOCON1_FSIZE_MASK) >> _CFD2FIFOCON1_FSIZE_POSITION;
        for (msgIndex = 0; msgIndex <= fifoSize; msgIndex++)
        {
            if ((can2MsgIndex[fifoNum-1] & (1 << (msgIndex & 0x1F))) == (1 << (msgIndex & 0x1F)))
            {
                can2MsgIndex[fifoNum-1] &= ~(1 << (msgIndex & 0x1F));
                break;
            }
        }
        /* Get a pointer to RX message buffer */
        rxMessage = (CANFD_RX_MSG_OBJECT *)PA_TO_KVA1(*(volatile uint32_t *)(&CFD2FIFOUA1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)));

        /* Check if it's a extended message type */
        if (rxMessage->r1 & CANFD_MSG_IDE_MASK)
        {
            *can2RxMsg[fifoNum-1][msgIndex].id = (((rxMessage->r0 & CANFD_MSG_RX_EXT_SID_MASK) << 18) | ((rxMessage->r0 & CANFD_MSG_RX_EXT_EID_MASK) >> 11)) & CANFD_MSG_EID_MASK;
        }
        else
        {
            *can2RxMsg[fifoNum-1][msgIndex].id = rxMessage->r0 & CANFD_MSG_SID_MASK;
        }

        if ((rxMessage->r1 & CANFD_MSG_RTR_MASK) && ((rxMessage->r1 & CANFD_MSG_FDF_MASK) == 0))
        {
            *can2RxMsg[fifoNum-1][msgIndex].msgAttr = CANFD_MSG_RX_REMOTE_FRAME;
        }
        else
        {
            *can2RxMsg[fifoNum-1][msgIndex].msgAttr = CANFD_MSG_RX_DATA_FRAME;
        }

        *can2RxMsg[fifoNum-1][msgIndex].size = dlcToLength[(rxMessage->r1 & CANFD_MSG_DLC_MASK)];

        if (can2RxMsg[fifoNum-1][msgIndex].timestamp != NULL)
        {
        }

        /* Copy the data into the payload */
        while (count < *can2RxMsg[fifoNum-1][msgIndex].size)
        {
            *can2RxMsg[fifoNum-1][msgIndex].buffer++ = rxMessage->data[count++];
        }

        /* Message processing is done, update the message buffer pointer. */
        *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) |= _CFD2FIFOCON1_UINC_MASK;

        if (((*(volatile uint32_t *)(&CFD2FIFOSTA1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) & _CFD2FIFOSTA1_TFNRFNIF_MASK) != _CFD2FIFOSTA1_TFNRFNIF_MASK) ||
            (can2MsgIndex[fifoNum-1] == 0))
        {
            *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) &= ~_CFD2FIFOCON1_TFNRFNIE_MASK;
        }
        can2Obj.errorStatus = 0;
    }
    IFS5CLR = _IFS5_CAN2IF_MASK;

    if (can2CallbackObj[fifoNum].callback != NULL)
    {
        can2CallbackObj[fifoNum].callback(can2CallbackObj[fifoNum].context);
    }
}

static void CAN2_TX_InterruptHandler(void)
{
    uint8_t  fifoNum = 0;

    fifoNum = (uint8_t)CFD2VEC & _CFD2VEC_ICODE_MASK;
    if (fifoNum <= CANFD_NUM_OF_FIFO)
    {
        if (fifoNum == 0)
        {
            CFD2TXQCON &= ~_CFD2TXQCON_TXQEIE_MASK;
        }
        else
        {
            *(volatile uint32_t *)(&CFD2FIFOCON1 + ((fifoNum - 1) * CANFD_FIFO_OFFSET)) &= ~_CFD2FIFOCON1_TFERFFIE_MASK;
        }
        can2Obj.errorStatus = 0;
    }
    IFS5CLR = _IFS5_CAN2IF_MASK;

    if (can2CallbackObj[fifoNum].callback != NULL)
    {
        can2CallbackObj[fifoNum].callback(can2CallbackObj[fifoNum].context);
    }
}

static void CAN2_MISC_InterruptHandler(void)
{
    uint32_t errorStatus = 0;

    CFD2INT &= ~(_CFD2INT_SERRIF_MASK | _CFD2INT_CERRIF_MASK | _CFD2INT_IVMIF_MASK);
    IFS5CLR = _IFS5_CAN2IF_MASK;
    errorStatus = CFD2TREC;

    /* Check if error occurred */
    can2Obj.errorStatus = ((errorStatus & _CFD2TREC_EWARN_MASK) |
                                                      (errorStatus & _CFD2TREC_RXWARN_MASK) |
                                                      (errorStatus & _CFD2TREC_TXWARN_MASK) |
                                                      (errorStatus & _CFD2TREC_RXBP_MASK) |
                                                      (errorStatus & _CFD2TREC_TXBP_MASK) |
                                                      (errorStatus & _CFD2TREC_TXBO_MASK));

    /* Client must call CAN2_ErrorGet and CAN2_ErrorCountGet functions to get errors */
    if (can2ErrorCallbackObj.callback != NULL)
    {
        can2ErrorCallbackObj.callback(can2ErrorCallbackObj.context);
    }
}

// *****************************************************************************
/* Function:
    void CAN2_InterruptHandler(void)

   Summary:
    CAN2 Peripheral Interrupt Handler.

   Description:
    This function is CAN2 Peripheral Interrupt Handler and will
    called on every CAN2 interrupt.

   Precondition:
    None.

   Parameters:
    None.

   Returns:
    None.

   Remarks:
    The function is called as peripheral instance's interrupt handler if the
    instance interrupt is enabled. If peripheral instance's interrupt is not
    enabled user need to call it from the main while loop of the application.
*/
void CAN2_InterruptHandler(void)
{
    /* Call CAN MISC interrupt handler if SERRIF/CERRIF/IVMIF interrupt flag is set */
    if (CFD2INT & (_CFD2INT_SERRIF_MASK | _CFD2INT_CERRIF_MASK | _CFD2INT_IVMIF_MASK))
    {
        CAN2_MISC_InterruptHandler();
    }

    /* Call CAN RX interrupt handler if RXIF interrupt flag is set */
    if (CFD2INT & _CFD2INT_RXIF_MASK)
    {
        CAN2_RX_InterruptHandler();
    }

    /* Call CAN TX interrupt handler if TXIF interrupt flag is set */
    if (CFD2INT & _CFD2INT_TXIF_MASK)
    {
        CAN2_TX_InterruptHandler();
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2007 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (��MStar Confidential Information��) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///
/// file    mhal_mbx_interrupt.c
/// @brief  MStar MailBox interrupt DDI
/// @author MStar Semiconductor Inc.
///////////////////////////////////////////////////////////////////////////////////////////////////

#define _MHAL_MBX_INTERRUPT_C

//=============================================================================
// Include Files
//=============================================================================
#include <linux/interrupt.h>
#include <linux/delay.h>

#include "mdrv_mstypes.h"
#include "mdrv_mbx.h"
#include "mhal_mbx_interrupt.h"

//=============================================================================
// Compile options
//=============================================================================


//=============================================================================
// Local Defines
//=============================================================================

//=============================================================================
// Debug Macros
//=============================================================================
#define MBXINT_DEBUG
#ifdef MBXINT_DEBUG
    #define MBXINT_PRINT(fmt, args...)      printk("[MailBox (Driver)][%05d] " fmt, __LINE__, ## args)
    #define MBXINT_ASSERT(_cnd, _fmt, _args...)                   \
                                    if (!(_cnd)) {              \
                                        MBXINT_PRINT(_fmt, ##_args);  \
                                    }
#else
    #define MBXINT_PRINT(_fmt, _args...)
    #define MBXINT_ASSERT(_cnd, _fmt, _args...)
#endif

//=============================================================================
// Macros
//=============================================================================

//=============================================================================
// Local Variables
//=============================================================================
static MBX_MSGRECV_CB_FUNC _pMBXMsgRecvCbFunc = NULL;
static U32 _MbxIntMask=0;

//=============================================================================
// Global Variables
//=============================================================================

//=============================================================================
// Local Function Prototypes
//=============================================================================
static void _MHAL_MBXINT_AEON2PMHandler(unsigned long unused);
static void _MHAL_MBXINT_AEON2MIPSHandler(unsigned long unused);
static void _MHAL_MBXINT_MIPS2PMHandler(unsigned long unused);
static void _MHAL_MBXINT_MIPS2AEONHandler(unsigned long unused);
static void _MHAL_MBXINT_PM2MIPSHandler(unsigned long unused);
static void _MHAL_MBXINT_PM2AEONHandler(unsigned long unused);

//callback from driver:
static irqreturn_t _MHAL_MBXINT_INTHandler(int irq, void *dev_id);

static MS_U16 _MapFiq2FiqMask(MS_S32 s32Fiq);

static MBX_Result _MHAL_MBXINT_SetHostCPU(MBX_CPU_ID eHostCPU);
static U32 _MHAL_MBXINT_GetIntFlgMask(U32 mbxInt);
static int _MHAL_MBXINT_IsIntFlgSet(U32 mbxInt);
static void _MHAL_MBXINT_SetIntFlg(U32 mbxInt);
static void _MHAL_MBXINT_ClearIntFlg(U32 mbxInt);
//=============================================================================
// Local Function
//=============================================================================

//-------------------------------------------------------------------------------------------------
/// Get Int flag mask
/// @param  eHostCPUID                  \b IN: The Host CPU ID
/// @return Flag bit mask
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
U32 _MHAL_MBXINT_GetIntFlgMask(U32 mbxInt)
{
    U32 flg=0;
    switch(mbxInt)
    {
    case MBX_INT_MIPS2PM:
        flg = 0x01;
        break;
    case MBX_INT_AEON2PM:
        flg = 0x02;
        break;
    case MBX_INT_PM2MIPS:
        flg = 0x04;
        break;
    case MBX_INT_AEON2MIPS:
        flg = 0x08;
        break;
    case MBX_INT_PM2AEON:
        flg = 0x10;
        break;
    case MBX_INT_MIPS2AEON:
        flg = 0x20;
        break;
    default:
        break;
    }
    return flg;
}
//-------------------------------------------------------------------------------------------------
/// Test if Int flag is set
/// @param  eHostCPUID                  \b IN: The Host CPU ID
/// @return !=0: set; ==0: unset
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
int _MHAL_MBXINT_IsIntFlgSet(U32 mbxInt)
{
    U32 flg=_MHAL_MBXINT_GetIntFlgMask(mbxInt);
    if(flg==0)return 0;
    return (_MbxIntMask&flg);
}
//-------------------------------------------------------------------------------------------------
/// set Int flag
/// @param  eHostCPUID                  \b IN: The Host CPU ID
/// @return void
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_SetIntFlg(U32 mbxInt)
{
    U32 flg=_MHAL_MBXINT_GetIntFlgMask(mbxInt);
    if(flg==0)return;
    _MbxIntMask |= flg;
}
//-------------------------------------------------------------------------------------------------
/// clear Int flag
/// @param  eHostCPUID                  \b IN: The Host CPU ID
/// @return void
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_ClearIntFlg(U32 mbxInt)
{
    U32 flg=_MHAL_MBXINT_GetIntFlgMask(mbxInt);
    if(flg==0)return;
    _MbxIntMask &= ~flg;
}


//-------------------------------------------------------------------------------------------------
/// Notify Interrupt Aeon2PM
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_AEON2PMHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_AEON2PM);
}

DECLARE_TASKLET(_mbxAeon2PMTaskletMBXINT, _MHAL_MBXINT_AEON2PMHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Notify Interrupt Aeon2Mips
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_AEON2MIPSHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_AEON2MIPS);
}

DECLARE_TASKLET(_mbxAeon2MipsTaskletMBXINT, _MHAL_MBXINT_AEON2MIPSHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Notify Interrupt Mips2PM
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_MIPS2PMHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_MIPS2PM);
}

DECLARE_TASKLET(_mbxMips2PMTaskletMBXINT, _MHAL_MBXINT_MIPS2PMHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Notify Interrupt Mips2Aeon
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_MIPS2AEONHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_MIPS2AEON);
}

DECLARE_TASKLET(_mbxMips2AeonTaskletMBXINT, _MHAL_MBXINT_MIPS2AEONHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Notify Interrupt PM2Mips
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_PM2MIPSHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_PM2MIPS);
}

DECLARE_TASKLET(_mbxPM2MipsTaskletMBXINT, _MHAL_MBXINT_PM2MIPSHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Notify Interrupt PM2Aeon
/// @param  unused                  \b IN: unused
/// @return void
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void _MHAL_MBXINT_PM2AEONHandler(unsigned long unused)
{
    if(NULL == _pMBXMsgRecvCbFunc)
    {
        return;
    }

    _pMBXMsgRecvCbFunc(MBX_INT_PM2AEON);
}

DECLARE_TASKLET(_mbxPM2AeonTaskletMBXINT, _MHAL_MBXINT_PM2AEONHandler, 0);

//-------------------------------------------------------------------------------------------------
/// Handle Interrupt, schedule tasklet
/// @param  irq                  \b IN: interrupt number
/// @param  dev_id                  \b IN: dev id
/// @return irqreturn_t: IRQ_HANDLED
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
irqreturn_t _MHAL_MBXINT_INTHandler(int irq, void *dev_id)
{
    switch(irq)
    {
        case E_FIQ_INT_AEON_TO_8051:
            tasklet_schedule(&_mbxAeon2PMTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_AEON2PM);
            break;
        case E_FIQ_INT_MIPS_TO_8051:
            tasklet_schedule(&_mbxMips2PMTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_MIPS2PM);
            break;
        case E_FIQ_INT_8051_TO_AEON:
            tasklet_schedule(&_mbxPM2AeonTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_PM2AEON);
            break;
        case E_FIQ_INT_MIPS_TO_AEON:
            tasklet_schedule(&_mbxMips2AeonTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_MIPS2AEON);
            break;
        case E_FIQ_INT_8051_TO_MIPS:
            tasklet_schedule(&_mbxPM2MipsTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_PM2MIPS);
            break;
        case E_FIQ_INT_AEON_TO_MIPS:
            tasklet_schedule(&_mbxAeon2MipsTaskletMBXINT);
            MHAL_MBXINT_Clear(MBX_INT_AEON2MIPS);
            break;
    }

    return IRQ_HANDLED;
}


//-------------------------------------------------------------------------------------------------
/// Fiq number 2 Fiq Mask transfer
/// @param  s32Fiq                  \b IN: fiq number
/// @return INT_FIQMASK_xx: mapped fiq mask
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MS_U16 _MapFiq2FiqMask(MS_S32 s32Fiq)
{
    switch(s32Fiq)
    {
        case MBX_INT_AEON2PM:
            return INT_FIQMASK_AEON2PM;
        case MBX_INT_MIPS2PM:
            return INT_FIQMASK_MIPS2PM;
        case MBX_INT_PM2AEON:
            return INT_FIQMASK_PM2AEON;
        case MBX_INT_MIPS2AEON:
            return INT_FIQMASK_MIPS2AEON;
        case MBX_INT_PM2MIPS:
            return INT_FIQMASK_PM2MIPS;
        case MBX_INT_AEON2MIPS:
            return INT_FIQMASK_AEON2MIPS;
        default:
            return 0;
    }
}

//-------------------------------------------------------------------------------------------------
/// Set Interrupt to Host CPU ID: Enable related interrupt and attached related callback.
/// @param  eHostCPUID                  \b IN: The Host CPU ID
/// @return E_MBX_SUCCESS
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result _MHAL_MBXINT_SetHostCPU(MBX_CPU_ID eHostCPU)
{
    MS_U32     s32Err;

    switch(eHostCPU)
    {
        case E_MBX_CPU_PM:
            s32Err = request_irq(E_FIQ_INT_AEON_TO_8051, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_AEON2PM", NULL);
            if(s32Err<0)
            {
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_AEON2PM);
                return E_MBX_UNKNOW_ERROR;
            }

            s32Err = request_irq(E_FIQ_INT_MIPS_TO_8051, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_MIPS2PM", NULL);
            if(s32Err<0)
            {
                free_irq(E_FIQ_INT_AEON_TO_8051, NULL);
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_MIPS2PM);
                return E_MBX_UNKNOW_ERROR;
            }

            MHAL_MBXINT_Enable(MBX_INT_AEON2PM, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_AEON2PM);
            MHAL_MBXINT_Enable(MBX_INT_MIPS2PM, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_MIPS2PM);
            break;
        case E_MBX_CPU_AEON:
            s32Err = request_irq(E_FIQ_INT_8051_TO_AEON, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_PM2AEON", NULL);
            if(s32Err<0)
            {
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_PM2AEON);
                return E_MBX_UNKNOW_ERROR;
            }

            s32Err = request_irq(E_FIQ_INT_MIPS_TO_AEON, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_MIPS2AEON", NULL);
            if(s32Err<0)
            {
                free_irq(E_FIQ_INT_8051_TO_AEON, NULL);
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_MIPS2AEON);
                return E_MBX_UNKNOW_ERROR;
            }
            MHAL_MBXINT_Enable(MBX_INT_PM2AEON, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_PM2AEON);
            MHAL_MBXINT_Enable(MBX_INT_MIPS2AEON, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_MIPS2AEON);
            break;
        case E_MBX_CPU_MIPS:
            s32Err = request_irq(E_FIQ_INT_8051_TO_MIPS, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_PM2MIPS", NULL);
            if(s32Err<0)
            {
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_PM2MIPS);
                return E_MBX_UNKNOW_ERROR;
            }

            s32Err = request_irq(E_FIQ_INT_AEON_TO_MIPS, _MHAL_MBXINT_INTHandler, SA_INTERRUPT, "MBX_FIQ_AEON2MIPS", NULL);
            if(s32Err<0)
            {
                free_irq(E_FIQ_INT_8051_TO_MIPS, NULL);
                MBXINT_PRINT("request FIQ: %x Failed!\n", MBX_INT_AEON2MIPS);
                return E_MBX_UNKNOW_ERROR;
            }
            MHAL_MBXINT_Enable(MBX_INT_PM2MIPS, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_PM2MIPS);
            MHAL_MBXINT_Enable(MBX_INT_AEON2MIPS, TRUE);
            _MHAL_MBXINT_SetIntFlg(MBX_INT_AEON2MIPS);
            break;
        default:
            return E_MBX_ERR_INVALID_CPU_ID;
    }

    return E_MBX_SUCCESS;
}

//=============================================================================
// Mailbox HAL Interrupt Driver Function
//=============================================================================

//-------------------------------------------------------------------------------------------------
/// Handle Interrupt INIT
/// @param  eHostCPU                  \b IN: interrupt owner
/// @param  pMBXRecvMsgCBFunc                  \b IN: callback func by driver
/// @return E_MBX_ERR_INVALID_CPU_ID: the cpu id is wrong
/// @return E_MBX_UNKNOW_ERROR: request_irq failed;
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Init (MBX_CPU_ID eHostCPU, MBX_MSGRECV_CB_FUNC pMBXRecvMsgCBFunc)
{
    _pMBXMsgRecvCbFunc = pMBXRecvMsgCBFunc;

    return _MHAL_MBXINT_SetHostCPU(eHostCPU);
}

//-------------------------------------------------------------------------------------------------
/// Handle Interrupt DeINIT
/// @param  eHostCPU                  \b IN: interrupt owner
/// @return void;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
void MHAL_MBXINT_DeInit (MBX_CPU_ID eHostCPU)
{
    switch(eHostCPU)
    {
        case E_MBX_CPU_PM:
            MHAL_MBXINT_Enable(MBX_INT_AEON2PM, FALSE);
            MHAL_MBXINT_Enable(MBX_INT_MIPS2PM, FALSE);
            free_irq(E_FIQ_INT_AEON_TO_8051, NULL);
            free_irq(E_FIQ_INT_MIPS_TO_8051, NULL);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_AEON2PM);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_MIPS2PM);
            break;
        case E_MBX_CPU_AEON:
            MHAL_MBXINT_Enable(MBX_INT_PM2AEON, FALSE);
            MHAL_MBXINT_Enable(MBX_INT_MIPS2AEON, FALSE);
            free_irq(E_FIQ_INT_8051_TO_AEON, NULL);
            free_irq(E_FIQ_INT_MIPS_TO_AEON, NULL);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_PM2AEON);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_MIPS2AEON);
            break;
        case E_MBX_CPU_MIPS:
            MHAL_MBXINT_Enable(MBX_INT_PM2MIPS, FALSE);
            MHAL_MBXINT_Enable(MBX_INT_AEON2MIPS, FALSE);
            free_irq(E_FIQ_INT_8051_TO_MIPS, NULL);
            free_irq(E_FIQ_INT_AEON_TO_MIPS, NULL);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_PM2MIPS);
            _MHAL_MBXINT_ClearIntFlg(MBX_INT_AEON2MIPS);
            break;
        default:
            break;
    }
}

//-------------------------------------------------------------------------------------------------
/// Suspend MBX interrupt
/// @param  eHostCPU                  \b IN: interrupt owner
/// @return E_MBX_ERR_INVALID_CPU_ID: the cpu id is wrong
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Suspend(MBX_CPU_ID eHostCPU)
{
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_MIPS2PM)){
        MHAL_MBXINT_Enable(MBX_INT_MIPS2PM,FALSE);
        disable_irq(E_FIQ_INT_MIPS_TO_8051);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_AEON2PM)){
        MHAL_MBXINT_Enable(MBX_INT_AEON2PM,FALSE);
        disable_irq(E_FIQ_INT_AEON_TO_8051);
    }

    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_PM2MIPS)){
        MHAL_MBXINT_Enable(MBX_INT_PM2MIPS,FALSE);
        disable_irq(E_FIQ_INT_8051_TO_MIPS);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_AEON2MIPS)){
        MHAL_MBXINT_Enable(MBX_INT_AEON2MIPS,FALSE);
        disable_irq(E_FIQ_INT_AEON_TO_MIPS);
    }

    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_PM2AEON)){
        MHAL_MBXINT_Enable(MBX_INT_PM2AEON,FALSE);
        disable_irq(E_FIQ_INT_8051_TO_AEON);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_MIPS2AEON)){
        MHAL_MBXINT_Enable(MBX_INT_MIPS2AEON,FALSE);
        disable_irq(E_FIQ_INT_MIPS_TO_AEON);
    }
    return E_MBX_SUCCESS;
}
//-------------------------------------------------------------------------------------------------
/// Resume MBX interrupt
/// @param  eHostCPU                  \b IN: interrupt owner
/// @return E_MBX_ERR_INVALID_CPU_ID: the cpu id is wrong
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Resume(MBX_CPU_ID eHostCPU)
{
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_MIPS2PM)){
        MHAL_MBXINT_Enable(MBX_INT_MIPS2PM,TRUE);
        enable_irq(E_FIQ_INT_MIPS_TO_8051);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_AEON2PM)){
        MHAL_MBXINT_Enable(MBX_INT_AEON2PM,TRUE);
        enable_irq(E_FIQ_INT_AEON_TO_8051);
    }

    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_PM2MIPS)){
        MHAL_MBXINT_Enable(MBX_INT_PM2MIPS,TRUE);
        enable_irq(E_FIQ_INT_8051_TO_MIPS);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_AEON2MIPS)){
        MHAL_MBXINT_Enable(MBX_INT_AEON2MIPS,TRUE);
        enable_irq(E_FIQ_INT_AEON_TO_MIPS);
    }

    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_PM2AEON)){
        MHAL_MBXINT_Enable(MBX_INT_PM2AEON,TRUE);
        enable_irq(E_FIQ_INT_8051_TO_AEON);
    }
    if(_MHAL_MBXINT_IsIntFlgSet(MBX_INT_MIPS2AEON)){
        MHAL_MBXINT_Enable(MBX_INT_MIPS2AEON,TRUE);
        enable_irq(E_FIQ_INT_MIPS_TO_AEON);
    }
    return E_MBX_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
/// Reset Host CPU for MBX Interrupt
/// @param  ePrevCPU                  \b IN: previous host cpu id
/// @param  eConfigCpu                  \b IN: new configed cpu id
/// @return E_MBX_SUCCESS: success;
/// @return E_MBX_INVALID_CPU_ID
/// @attention
/// <b>[MXLIB] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_ResetHostCPU (MBX_CPU_ID ePrevCPU, MBX_CPU_ID eConfigCpu)
{
    MHAL_MBXINT_DeInit(ePrevCPU);

    return _MHAL_MBXINT_SetHostCPU(eConfigCpu);
}

//-------------------------------------------------------------------------------------------------
/// enable/disable Interrupt
/// @param  s32Fiq                  \b IN: interrupt for enable/disable
/// @param  bEnable                  \b IN: Enable or Disable
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Enable (MS_S32 s32Fiq, MS_BOOL bEnable)
{
    MS_U16 u16Val;
    MS_U16 u16FIQMask = _MapFiq2FiqMask(s32Fiq);

    mb();
    u16Val = AEON1_INT_REG(REG_INT_FIQMASK_L);

    if(bEnable)
    {
        AEON1_INT_REG(REG_INT_FIQMASK_L) = u16Val & ~(u16FIQMask);
    }
    else
    {
        AEON1_INT_REG(REG_INT_FIQMASK_L) = u16Val | (u16FIQMask);
    }

    mb();

    return E_MBX_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
/// Fire Interrupt
/// @param  dstCPUID                  \b IN: dst cpu of interrupt
/// @param  srcCPUID                  \b IN: src cpu of interrupt
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Fire (MBX_CPU_ID dstCPUID, MBX_CPU_ID srcCPUID)
{
    MBXINT_ASSERT((dstCPUID!=srcCPUID),"dst cpu is the same as src cpu!\n");

    switch(srcCPUID)
    {
        case E_MBX_CPU_PM:
            if(dstCPUID==E_MBX_CPU_AEON)
            {
                INT_REG(REG_INT_PMFIRE) = INT_PM2AEON;
            }
            else
            {
                INT_REG(REG_INT_PMFIRE) = INT_PM2MIPS;
            }
            mb();
            INT_REG(REG_INT_PMFIRE) = 0;
            mb();
            break;
        case E_MBX_CPU_AEON:
            if(dstCPUID==E_MBX_CPU_PM)
            {
                INT_REG(REG_INT_AEONFIRE) = INT_AEON2PM;
            }
            else
            {
                INT_REG(REG_INT_AEONFIRE) = INT_AEON2MIPS;
            }
            mb();
            INT_REG(REG_INT_AEONFIRE) = 0;
            mb();
            break;
        case E_MBX_CPU_MIPS:
            if(dstCPUID==E_MBX_CPU_PM)
            {
                AEON1_INT_REG(REG_INT_MIPSFIRE) = INT_MIPS2PM;
            }
            else
            {
                AEON1_INT_REG(REG_INT_MIPSFIRE) = INT_MIPS2AEON;
            }
            mb();
            AEON1_INT_REG(REG_INT_MIPSFIRE) = 0;
            mb();
            break;
        default:
            MBXINT_ASSERT(FALSE,"wrong src cpu!\n");
            break;
    }

    return E_MBX_SUCCESS;
}

//-------------------------------------------------------------------------------------------------
/// clear Interrupt
/// @param  s32Fiq                  \b IN: interrupt number
/// @return E_MBX_SUCCESS: success;
/// @attention
/// <b>[OBAMA] <em></em></b>
//-------------------------------------------------------------------------------------------------
MBX_Result MHAL_MBXINT_Clear (MS_S32 s32Fiq)
{
    MS_U16 u16Val;
    MS_U16 u16FIQMask = _MapFiq2FiqMask(s32Fiq);

    mb();
    u16Val = AEON1_INT_REG(REG_INT_FIQCLEAR_L);

    AEON1_INT_REG(REG_INT_FIQCLEAR_L) = u16Val | (u16FIQMask);
    mb();
    udelay(10);
    AEON1_INT_REG(REG_INT_FIQCLEAR_L) = u16Val & ~(u16FIQMask);

    mb();

    return E_MBX_SUCCESS;
}

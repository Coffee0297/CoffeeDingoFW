#include "param_protocol.h"
#include "dingopdm_config.h"
#include "config.h"
#include "mailbox.h"
#include "config_handler.h"
#include <cstring>

extern PdmConfig stConfig;

uint16_t nNumWriteParams = 0;
uint16_t nNumReadParams = 0;

void DecodeParamCmd(CANRxFrame *rx, ParamMsg *out)
{
    out->eCmd = static_cast<MsgCmd>(rx->data8[0]);
    out->nIndex = rx->data8[1] | (rx->data8[2] << 8);
    out->nSubIndex = rx->data8[3];
    out->nValue =  rx->data8[4] |
                   rx->data8[5] << 8 |
                   rx->data8[6] << 16 |
                  (rx->data8[7]) << 24;
}

void EncodeParamRsp(CANTxFrame *tx, uint8_t cmd, uint16_t index, uint8_t subindex, uint32_t value)
{
    tx->IDE = 0;
    tx->RTR = 0;
    tx->DLC = 8;

    tx->SID = stConfig.stDevConfig.nBaseId + TX_SETTINGS_ID_OFFSET;

    tx->data8[0] = cmd;
    tx->data8[1] = index & 0xFF;
    tx->data8[2] = (index >> 8) & 0xFF;
    tx->data8[3] = subindex;
    tx->data8[4] = value & 0xFF;
    tx->data8[5] = (value >> 8) & 0xFF;
    tx->data8[6] = (value >> 16) & 0xFF;
    tx->data8[7] = (value >> 24) & 0xFF;
}

void SendAllParams(bool modifiedOnly) {
    CANTxFrame tx;

    for (int i = 0; i < NUM_PARAMS; i++) {
        if (modifiedOnly && IsDefaultValue(&stParams[i])) {
            continue; // Skip default params if only sending modified
        }
        uint32_t value = ReadParam(&stParams[i]);
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadAllRsp), 
                        stParams[i].nIndex, stParams[i].nSubIndex, value);
        PostTxFrame(&tx);

        nNumReadParams++;
        
        // Small delay to avoid saturating mailbox
        chThdSleepMilliseconds(1);
    }

    chThdSleepMilliseconds(1);
    EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadAllComplete), nNumReadParams, 0, 0); // End of params marker, return number of params sent
    PostTxFrame(&tx);
}

void SetAllDefaultParams(bool temp) {
    for (int i = 0; i < NUM_PARAMS; i++) {
        WriteParam(&stParams[i], stParams[i].nDefaultVal, temp);
    }
}

void ApplyTempParams() {
    for (int i = 0; i < NUM_PARAMS; i++) {
        uint32_t tempVal = ReadParam(&stParams[i], true);
        WriteParam(&stParams[i], tempVal, false);
    }
}

void ProcessParamMsg(CANRxFrame *rx) {
    CANTxFrame tx;
    ParamMsg msg;

    if (rx->SID != stConfig.stDevConfig.nBaseId - 1)
        return;

    if (rx->DLC != 8)
        return;

    msg.eCmd = static_cast<MsgCmd>(rx->data8[0]);

    if (!(msg.eCmd == MsgCmd::Read || 
        msg.eCmd == MsgCmd::Write || 
        msg.eCmd == MsgCmd::ReadAll ||
        msg.eCmd == MsgCmd::ReadAllRsp ||
        msg.eCmd == MsgCmd::ReadAllComplete ||
        msg.eCmd == MsgCmd::WriteAll ||
        msg.eCmd == MsgCmd::WriteAllVal ||
        msg.eCmd == MsgCmd::WriteAllComplete))
        return;

    DecodeParamCmd(rx, &msg);

    if (msg.eCmd == MsgCmd::Read) {
        const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
        if (param) {
            uint32_t value = ReadParam(param);
            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::Read), msg.nIndex, msg.nSubIndex, value);
            PostTxFrame(&tx);
            return;
        }
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadParamNotFound), msg.nIndex, msg.nSubIndex, 0);
        PostTxFrame(&tx);
        return;
    }

    if (msg.eCmd == MsgCmd::Write) {
        const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
        if (param && WriteParam(param, msg.nValue)) {
            uint32_t value = ReadParam(param);
            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::Write), msg.nIndex, msg.nSubIndex, value);
            PostTxFrame(&tx);
            return;
        }
    }

    if ((msg.eCmd == MsgCmd::ReadAll) || 
        (msg.eCmd == MsgCmd::ReadAllModified)) {
        nNumReadParams = 0;
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::ReadAllModified), 0, 0, 0); // Start of params marker
        PostTxFrame(&tx);
        chThdSleepMilliseconds(1);
        SendAllParams(msg.eCmd == MsgCmd::ReadAllModified);
        return;
    }

    if ((msg.eCmd == MsgCmd::WriteAll) ||
        (msg.eCmd == MsgCmd::WriteAllModified)) {
        nNumWriteParams = 0;
        SetAllDefaultParams(true); // Clear temp values
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAll), 0, 0, 0); // Start of params marker
        PostTxFrame(&tx);
        return;
    }

    if (msg.eCmd == MsgCmd::WriteAllVal) {
        const ParamInfo* param = FindParam(msg.nIndex, msg.nSubIndex);
        //Param not found or invalid value, respond with error
        if (!param){
            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllParamNotFound), msg.nIndex, msg.nSubIndex, 0);
            PostTxFrame(&tx);
            return;
        }
        //Param out of range, respond with error
        if (!WriteParam(param, msg.nValue, true)) {
            EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllOutOfRange), msg.nIndex, msg.nSubIndex, msg.nValue);
            PostTxFrame(&tx);
            return;
        }
        //Param found, in range, staged successfully, respond with value for confirmation
        uint32_t value = ReadParam(param, true);
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllVal), msg.nIndex, msg.nSubIndex, value);
        PostTxFrame(&tx);
        nNumWriteParams++;
        return;
    }

    if (msg.eCmd == MsgCmd::WriteAllComplete) {
        // Ensure all params were written
        uint16_t nExpectedParams = rx->data8[1] | (rx->data8[2] << 8);
        if (nNumWriteParams == nExpectedParams) {
            ApplyTempParams();
        }
        EncodeParamRsp(&tx, static_cast<uint8_t>(MsgCmd::WriteAllComplete), nNumWriteParams, 0, 0); // End of params marker, return number of params written
        PostTxFrame(&tx);
        return;
    }
}
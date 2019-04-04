//----------------------------------------------------------------------------
//   The confidential and proprietary information contained in this file may
//   only be used by a person authorised under and to the extent permitted
//   by a subsisting licensing agreement from ARM Limited or its affiliates.
//
//          (C) COPYRIGHT 2018 ARM Limited or its affiliates
//              ALL RIGHTS RESERVED
//
//   This entire notice must be reproduced on all copies of this file
//   and copies of this file may only be made by a person if such person is
//   permitted to do so under the terms of a subsisting license agreement
//   from ARM Limited or its affiliates.
//----------------------------------------------------------------------------

#define LOG_CONTEXT "TransferPacket"

#include <BusManager/TransferPacket.h>

namespace act {

    armcpp11::Atomic<baseid> CTransferPacket::id_counter(0);

    CTransferPacket::CTransferPacket() : id(id_counter.FetchAdd(1))  {
        data.clear() ;
        mask.clear() ;
        state = EPacketProcessing ;
        notify = EPacketNotifyAlways ;
        address = 0 ;
        timeout_ms = 10000; // default timeout 10 seconds
    }

    CTransferPacket::~CTransferPacket() {
        data.clear() ;
        mask.clear() ;
    }

    const std::string CTransferPacket::GetObjectStaticName() {
        return LOG_CONTEXT ;
    }

    CATLError CTransferPacket::Validate() {
        return (EPacketSuccess == state) ? EATLErrorOk : EATLErrorInvalidValue;
    }

    void CTransferPacket::SetValue(UInt16 value, basesize offset) {
        data[offset+0] = (UInt8)((value>> 0)&0xFF);
        data[offset+1] = (UInt8)((value>> 8)&0xFF);
    }

    void CTransferPacket::SetValue(UInt32 value, basesize offset) {
        data[offset+0] = (UInt8)((value>> 0)&0xFF);
        data[offset+1] = (UInt8)((value>> 8)&0xFF);
        data[offset+2] = (UInt8)((value>>16)&0xFF);
        data[offset+3] = (UInt8)((value>>24)&0xFF);
    }

    UInt16 CTransferPacket::GetValue16(basesize offset) const {
        return (UInt16)(data[offset+0]) | ((UInt16)data[offset+1] << 8);
    }

    UInt32 CTransferPacket::GetValue32(basesize offset) const {
        return (UInt32)(data[offset+0]) | ((UInt32)data[offset+1] << 8) | ((UInt32)data[offset+2] << 16) | ((UInt32)data[offset+3] << 24);
    }

    void CTransferPacket::Invoke() {
        listener.Invoke(this);
    }

}

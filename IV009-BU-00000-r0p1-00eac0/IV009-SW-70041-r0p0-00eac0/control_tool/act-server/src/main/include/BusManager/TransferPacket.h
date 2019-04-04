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

#ifndef __TRANSFER_PACKET_H__
#define __TRANSFER_PACKET_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLLogger.h>
#include <ATL/ATLTemplates.h>

#include <vector>
#include <ARMCPP11/Atomic.h>

using namespace atl ;

namespace act {

    static const flags32 BUS_HW = 0x6877;
    static const flags32 BUS_FW = 0x6677;

    inline static const std::string getBusPrefix(const flags32& options) {
        std::string prefix;
        switch (options) {
            case BUS_HW: prefix = "hw-"; break;
            case BUS_FW: prefix = "fw-"; break;
            default:
                SysLog()->LogError(LOG_CONTEXT, "invalid bus manager option: " FSIZE_T, options);
                CATLError::SetLastError(EATLErrorInvalidParameters);
                return "";
        }
        CATLError::SetLastError(EATLErrorOk);
        return prefix;
    }

    typedef enum _EPacketCommand {
        EPacketWrite = 0,
        EPacketRead,
        EPacketTransaction
    } EPacketCommand ;

    typedef enum _EPacketState {
        EPacketProcessing = 0,
        EPacketInitialized,
        EPacketInvalid,
        EPacketInvalidated,
        EPacketSuccess,
        EPacketNoAnswer,
        EPacketNoAck,
        EPacketFail,
        EPacketLast
    } EPacketState ;

    typedef enum _EPacketNotify {
        EPacketNotifyAlways = 0,
        EPAcketNotifyNever,
        EPacketNotifyOnError,
        EPacketNotifyOnRead,
        EPacketNotifyOnWrite
    } EPacketNotify ;

    class CTransferPacket : public CATLObject {
    public :
        const baseid id;            // unique id for each packet
        EPacketState state;         // current state of the packet
        EPacketCommand command;     // packet command - write, read etc.
        EPacketNotify notify;       // type of notification for the packet
        baseaddr address;           // address to read/write opeartion
        std::vector<UInt8> data;  // input/output array for the command
        std::vector<UInt8> mask;  // mask for each byte of data. Only for write
        baseid reg_id;              // an unique register id
        UInt32 timeout_ms;        // transaction timeout in milliseconds
        CEventListener<> listener;
    public:
        CTransferPacket();
        CATLError Validate();

        void SetValue(UInt16 value, basesize offset);
        void SetValue(UInt32 value, basesize offset);
        UInt16 GetValue16(basesize offset) const;
        UInt32 GetValue32(basesize offset) const;

        void Invoke();

    protected:
        virtual ~CTransferPacket();

    protected:
        static armcpp11::Atomic<baseid> id_counter;
    public:
        // IATLObject
        virtual const std::string GetObjectStaticName();
    protected:
    };
}

#endif // __TRANSFER_PACKET_H__

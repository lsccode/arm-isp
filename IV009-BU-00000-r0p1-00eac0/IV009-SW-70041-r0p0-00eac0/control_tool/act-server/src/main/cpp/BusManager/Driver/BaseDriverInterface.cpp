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

#define LOG_CONTEXT "IDriver"

#include <BusManager/BaseDriverInterface.h>

using namespace std;
using namespace atl;

namespace act {

    CATLError CBaseDriver::AddListerner (EACTDriverListenerTypes event_name, IATLObserver* proc) {
        if (event_name == EACTForwardReply) {
            OnReplyReceived += proc;
            return EATLErrorOk;
        }
        return atl::EATLErrorInvalidParameters;
    }

    CATLError CBaseDriver::RemoveListerner (EACTDriverListenerTypes event_name, IATLObserver* proc) {
        if (event_name == EACTForwardReply) {
            OnReplyReceived -= proc;
            return EATLErrorOk;
        }
        return EATLErrorInvalidParameters;
    }

    CATLError CBaseDriver::NotifyListerners (TSmartPtr< CTransferPacket > packet) {
        EPacketNotify ntf = packet->notify;
        EPacketCommand cmd = packet->command;
        EPacketState stt = packet->state;
        if ((ntf == EPacketNotifyAlways) ||
            (ntf == EPacketNotifyOnError && stt != EPacketSuccess) ||
            (ntf == EPacketNotifyOnRead  && cmd == EPacketRead) ||
            (ntf == EPacketNotifyOnWrite && cmd == EPacketWrite)) {
            OnReplyReceived.Invoke( packet );
        }
        return EATLErrorOk;
    }
}

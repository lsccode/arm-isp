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

#ifndef __BUS_MANAGER_H__
#define __BUS_MANAGER_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>

#include "TransferPacket.h"
#include "BaseDriverInterface.h"

#include <string>
#include <deque>
#include <ARMCPP11/Mutex.h>
#include <ARMCPP11/ConditionVariable.h>

using namespace atl;

namespace act {

    typedef enum _EACTBusMode {
        EACTBusASyncMode = 0,
        EACTBusSyncMode
    } EACTBusMode;

    class CBusManager :  public virtual CATLObject, public virtual IATLComponent {
    public:
        static const UInt32 CBUS_MANAGER_MAX_QUEUE_SIZE = 100;
    protected:
        TSmartPtr<IDriver> driver;
        EACTBusMode busMode;
        std::deque< TSmartPtr< CTransferPacket > > packetDeque;
        armcpp11::RecursiveMutex sync;
        armcpp11::ConditionVariable syncRequest;

    public:
        CBusManager() : busMode(EACTBusSyncMode) {}
        virtual ~CBusManager() {}

        // IATLObject
        inline virtual const std::string GetObjectStaticName()  {return "CBusManager";}

        // IATLComponent
        inline virtual const std::string GetName() {return "CBusManager";}
        virtual CATLError Initialize(const flags32 &options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        // Bus interface
        virtual CATLError ProcessRequest(TSmartPtr<CTransferPacket> packet);
        virtual const std::vector<std::string> GetSupportedDrivers();
        virtual const std::vector<std::string> GetSupportedDevices();

        TSmartPtr<CTransferPacket> GetLastPacket();

        // callback interface for async mode
        CEventListener<> OnReplyReceived;

    protected:
        void ProcessAsyncReply( void *sender );
        void DriverCallback( void *sender );
    };

}

#endif // __BUS_MANAGER_H__

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

#ifndef __SOCKET_DRIVER_H__
#define __SOCKET_DRIVER_H__

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#endif

#include <cstdlib>
#include <vector>
#include <map>
#include <ARMCPP11/Chrono.h>
#include <ARMCPP11/Thread.h>
#include <ARMCPP11/Atomic.h>
#include <ARMCPP11/Mutex.h>

#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>

#include <BusManager/BaseDriverInterface.h>

namespace act {

    class CSocketDriver : public virtual CBaseDriver, public virtual CATLObject {
    public:
        CSocketDriver();
        ~CSocketDriver();

        // IATLObject interface
        virtual const std::string GetObjectStaticName();

        // IATLComponent
        virtual const std::string GetName();
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        // IDriver
        virtual CATLError ProcessPacket( TSmartPtr< CTransferPacket > packet, const flags32& options = 0) ;
        virtual const std::vector<std::string> GetDeviceList();
        virtual const std::vector<EPacketCommand> GetCommandList();
        virtual const EACTDriverMode GetCurrentMode();
        virtual const std::vector<EACTDriverMode> GetSupportedModes();

    private:
        // socket
        SOCKET soc;
        armcpp11::Mutex tx_mutex; // mutex locks sender access to the socket
        std::string dev_name;

        struct CTransaction {
            const armcpp11::HighResolutionClock::timepoint sent_time;    // here we keep time when transaction has been put to list (for timeouts)
            TSmartPtr<CTransferPacket> packet;                            // the packet itself
            CTransaction(TSmartPtr<CTransferPacket>& pack) : sent_time(armcpp11::HighResolutionClock::Now()), packet(pack) {}
        };

        // RX part
        armcpp11::Atomic<bool> rx_thread_stop;        // flag to stop the receiver thread
        armcpp11::Thread* rx_thread;        // receiver thread itself
        armcpp11::Mutex rx_mutex;        // mutex locks transfer list
        typedef std::map<UInt32,CTransaction> list_type;
        list_type transfer_list;    // here we store all sent transactions

        void create_rx_thread();    // create receiver thread
        void stop_rx_thread();        // stop receiver thread
        void rx_thread_processing();// receiver thread itself
        static void* rx_thread_wrapper(void *arg);

        CATLError PushTransaction(TSmartPtr<CTransferPacket>& packet);    // this function will process transaction
    };
}
#endif /* __SOCKET_DRIVER_H__ */

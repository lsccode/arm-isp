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

#ifndef OFFLINEDRIVER_H
#define OFFLINEDRIVER_H

#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>

#include <BusManager/BaseDriverInterface.h>

#include <map>

namespace act {

    class COfflineDriver : public virtual CBaseDriver, public virtual CATLObject {
    public:
        COfflineDriver() {}
        ~COfflineDriver() {}

        // IATLObject interface
        virtual const std::string GetObjectStaticName();

        // IATLComponent
        virtual const std::string GetName();
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        // IDriver
        virtual CATLError ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32& options = 0);
        virtual const std::vector<std::string> GetDeviceList();
        virtual const std::vector<EPacketCommand> GetCommandList();
        virtual const EACTDriverMode GetCurrentMode();
        virtual const std::vector<EACTDriverMode> GetSupportedModes();

    private:

        std::map < baseaddr, std::vector<UInt8> > isp;
        std::map < UInt16, UInt32 > firmware;
        std::map < UInt32, UInt8 > debug_registers;
        basebool debug_registers_reset;
        baseaddr dbg_addr;

        UInt32 fw_dbg_read_data() const;
        void fw_dbg_write_data(const UInt32& value);

    };

}

#endif // OFFLINEDRIVER_H


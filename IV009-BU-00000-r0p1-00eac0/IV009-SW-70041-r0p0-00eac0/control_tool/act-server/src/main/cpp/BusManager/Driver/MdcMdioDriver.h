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

#ifndef __MDC_MDIO_DRIVER_H__
#define __MDC_MDIO_DRIVER_H__

#include <vector>
#include <ARMCPP11/Mutex.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>

#include <BusManager/BaseDriverInterface.h>

#include "FTDIDriver.h"

namespace act {

    class CMdcMdioDriver : public virtual CBaseDriver, public virtual CATLObject {
    protected:
        // list of connected devices
        std::vector< std::string > devices;
    public:
        // IATLObject interface
        virtual const std::string GetObjectStaticName();

        // IATLComponent
        virtual const std::string GetName();

        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        virtual CATLError ProcessPacket(TSmartPtr< CTransferPacket > packet, const flags32& options = 0);
        virtual const std::vector<std::string> GetDeviceList();
        virtual const std::vector<EPacketCommand> GetCommandList();
        virtual const EACTDriverMode GetCurrentMode();
        virtual const std::vector<EACTDriverMode> GetSupportedModes();

    private:
        static const UInt32 alignment_mask = 3;

        FTDI ftdi;
        bool ftdi_opened;
        std::string dev_name;
        baseaddr mem_offset;
        CATLError read_data (TSmartPtr<CTransferPacket>& packet);
        CATLError write_data(TSmartPtr<CTransferPacket>& packet);
        void configure();
        std::vector<UInt8> data;
        armcpp11::Mutex mtx;

        // bitbang
        UInt8 bitbang_scl;
        UInt8 bitbang_sda_in;
        UInt8 bitbang_sda_out;
        UInt32 baudrate;
        void put_bit(UInt8 bit);
        void write_word(baseaddr addr, UInt32 val);
        UInt32 read_word(baseaddr addr);
        void write_mem(baseaddr addr, UInt32 val);
        UInt32 read_mem(baseaddr addr);
        void put_preamble();
        void put_start();
    };
}
#endif /* __MDC_MDIO_DRIVER_H__ */

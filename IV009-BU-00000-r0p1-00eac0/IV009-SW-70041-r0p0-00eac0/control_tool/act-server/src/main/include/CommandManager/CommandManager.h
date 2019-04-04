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

#ifndef __COMMAND_MANAGER__
#define __COMMAND_MANAGER__

#include <ATL/ATLTypes.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>
#include <ATL/ATLTemplates.h>
#include <AccessManager/AccessManager.h>
#include <BusManager/TransferPacket.h>
#include <BusManager/BusManager.h>

#include "RequestParser.h"
#include "ResponseWriter.h"

#include <string>
#include <queue>
#include <vector>
#include <ARMCPP11/Mutex.h>

using namespace atl;

namespace act {

    typedef enum _ERequestType {
        ERegRead = 0,
        ERegWrite,
        EApiRead,
        EApiWrite,
        EApiArrayRead,
        EApiArrayWrite,
        EInvalidCommand
    } ERequestType;


    class CCommandManager : public virtual CATLObject, public virtual IATLComponent {
    private:
        TSmartPtr<CAccessManager> am;
        armcpp11::RecursiveMutex sync;
        std::string documentRoot;

    public:

        CCommandManager();
        ~CCommandManager();

        virtual const std::string GetObjectStaticName();

        const std::string GetName();
        CATLError Initialize(const flags32& options = 0);
        CATLError Open();
        CATLError Close();
        CATLError Terminate();

        const std::pair<std::string, UInt16> proxy(const std::string& text, const UInt8& route);
        const std::pair<std::string, UInt16> uploadFile(const std::vector<UInt8> & data, const UInt8& sec, const UInt8& cmd);
        const std::pair<std::string, UInt16> downloadFile(const UInt8& sec, const UInt8& cmd);

        CATLError saveXml(const std::string& text);
        CATLError saveApi(const std::string& text);
        CATLError dropApi();
        CATLError configAccessManager(const std::string &text);

        basebool isAcessManagerConfigured() const;
        const std::string hwAccessType() const;
        const std::string fwAccessType() const;

    private:
        CATLError updateAccessManager(TSmartPtr<CAccessManagerConfig> amc);

    private:

        const std::pair< std::string, UInt16 > getErrorResponse (const CATLError& result);

        std::pair< CATLError, TSmartPtr<CRegRequest> > sendRegReadReq (const std::string&, const baseaddr&, const basesize&);
        std::pair< CATLError, TSmartPtr<CRegRequest> > sendRegWriteReq (const std::string&, const baseaddr &, const std::vector<UInt8>&, const std::vector<UInt8>&);
        std::pair< CATLError, TSmartPtr<CFWRequest> > sendApiReq(const EFWRequestType&, const UInt8&, const UInt8&, const UInt32&);
        CATLError processRegReq (TSmartPtr<CRegRequest> req, TSmartPtr<CResponseWriter> answer, const std::string& key = "bytes");
        CATLError processApiReq (TSmartPtr<CFWRequest> req, TSmartPtr<CResponseWriter> answer, const std::string& key = "val");

        CATLError CommandRegRead(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandRegPoll(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandRegWrite(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);

        CATLError CommandApiRead(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandApiPoll(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandApiWrite(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandLutRead(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);
        CATLError CommandLutWrite(const TSmartPtr<CRequestParser> &, TSmartPtr<CResponseWriter>);

    public:
        basebool isXMLLoaded() const;

    private:
        static const UInt8 HW_READ;     // POST "/hw/read"
        static const UInt8 HW_POLL;     // POST "/hw/poll"
        static const UInt8 HW_WRITE;    // POST "/hw/write"
        static const UInt8 FW_GET;      // POST "/fw/get"
        static const UInt8 FW_SET;      // POST "/fw/set"
        static const UInt8 CA_LOAD;     // POST "/calibration/load"
        static const UInt8 CA_SAVE;     // POST "/calibration/save"
        static const UInt8 FW_POLL;     // POST "/fw/poll"
    };
}

#endif // __COMMAND_MANAGER__

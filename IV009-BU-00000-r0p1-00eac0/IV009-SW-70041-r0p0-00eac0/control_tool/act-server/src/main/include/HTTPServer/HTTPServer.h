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

#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>
#include <ATL/ATLComponent.h>

using namespace atl;

namespace act {

     typedef enum __EHTTPServerStates {
        EHTTPServerNotInitialized = 0,
        EHTTPServerInitialized,
        EHTTPServerOpened,
        EHTTPServerRunning,
        EHTTPServerStopping,
        EHTTPServerStopped,
        EHTTPServerClosed
    } EHTTPServerStates;


    class CHTTPServer : public virtual CATLObject, public virtual IATLServer {
    private:
        TSmartPtr< CCommandManager > cm;  // command manager
        TSmartPtr< CEventEngine > engine; // http engine
        EHTTPServerStates state;

    public:
        CHTTPServer() : state(EHTTPServerNotInitialized) {}
        virtual ~CHTTPServer() {}

        // IATLObject
        inline const std::string GetObjectStaticName() {return "CHTTPServer";}

        // IATLComponent
        inline const std::string GetName() {return "CHTTPServer";}
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        // IATLServer
        virtual void ServerFinish(void*);

    };
}

#endif // __HTTP_SERVER_H__

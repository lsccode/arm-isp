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

#ifndef __EVENT_ENGINE_H__
#define __EVENT_ENGINE_H__

#include <ATL/ATLTypes.h>
#include <ATL/ATLError.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLComponent.h>

#include <CommandManager/CommandManager.h>

#include <string>
#include <map>

using namespace atl;

namespace act {

    typedef enum __EHTTPEngineStates {
        EHTTPEngineNotInitialized = 0,
        EHTTPEngineInitialized,
        EHTTPEngineClosed,
        EHTTPEngineStopped,
        EHTTPEngineRunning,
        EHTTPEngineStopping,
    } EHTTPEngineStates;

    class CEventEngine : public virtual CATLObject, public virtual IATLServer {
    private:
        EHTTPEngineStates state;
        struct event_base *eventBase;
        struct evhttp *eventHttp;
        TSmartPtr<CCommandManager> cm;
        std::string documentRoot;
    private:
        static void baseEventHandler(struct evhttp_request *req, void *arg);
        static void checkResult(const CATLError& res, struct evhttp_request *req);
        static CATLError getPayload(struct evhttp_request *req, std::string& payload);
        static CATLError getPayload(evhttp_request *req, std::vector<UInt8>& payload);

    public:
        CEventEngine(TSmartPtr<CCommandManager> cm) : CATLObject(), state(EHTTPEngineNotInitialized), eventBase(NULL), eventHttp(NULL), cm(cm) {}
        virtual ~CEventEngine();

        // IATLObject
        virtual const std::string GetObjectStaticName();

        // IATLComponent
        virtual const std::string GetName();
        virtual CATLError Initialize(const flags32& options = 0);
        virtual CATLError Open();
        virtual CATLError Close();
        virtual CATLError Terminate();

        CATLError Run();

        EHTTPEngineStates GetState();

        // IATLServer
        virtual void ServerFinish(void*);

    private:
        static const char *guessContentType(const char *path);
        static CATLError addFileToSend(struct evbuffer *evb, std::string full_path);

        static const UInt8 GET_ANY;   // GET      "/**/*"
        static const UInt8 GET_ROOT;  // GET      "/"
        static const UInt8 GET_VER;   // GET      "/version"
        static const UInt8 ACC_MGR;   // GET/POST "/am"
        static const UInt8 GET_ICO;   // GET      "/favicon.ico"
        static const UInt8 UPLD_XML;  // POST     "/xml"
        static const UInt8 UPLD_API;  // POST     "/api"
        static const UInt8 GET_LOAD;  // GET      "/reload"
        static const UInt8 GET_RDIR;  // GET      redirect to /
        static const UInt8 GET_INFO;  // GET      "/about"

        static const UInt8 HW_READ;   // POST "/hw/read"
        static const UInt8 HW_POLL;   // POST "/hw/poll"
        static const UInt8 HW_WRITE;  // POST "/hw/write"
        static const UInt8 FW_GET;    // POST "/fw/get"
        static const UInt8 FW_SET;    // POST "/fw/set"
        static const UInt8 CA_LOAD;   // POST "/calibration/load"
        static const UInt8 CA_SAVE;   // POST "/calibration/save"
        static const UInt8 FW_POLL;   // POST "/fw/poll"
        static const UInt8 FI_DNLOAD; // POST "/file/download"
        static const UInt8 FI_UPLOAD; // POST "/file/upload"

    };

}

#endif // __EVENT_ENGINE_H__

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

#define LOG_CONTEXT "CommandManager"

#include <CommandManager/CommandManager.h>

#include <BusManager/BusManager.h>
#include <ATL/ATLLogger.h>
#include <ATL/ATLConfig.h>

#include <iostream>
#include <fstream>
#include <assert.h>

namespace act {

    const UInt8 CCommandManager::HW_READ   = 0x80;
    const UInt8 CCommandManager::HW_POLL   = 0x81;
    const UInt8 CCommandManager::HW_WRITE  = 0x82;
    const UInt8 CCommandManager::FW_GET    = 0x83;
    const UInt8 CCommandManager::FW_SET    = 0x84;
    const UInt8 CCommandManager::CA_LOAD   = 0x85;
    const UInt8 CCommandManager::CA_SAVE   = 0x86;
    const UInt8 CCommandManager::FW_POLL   = 0x87;

    CCommandManager::CCommandManager() {}

    CCommandManager::~CCommandManager() {}

    const std::string CCommandManager::GetObjectStaticName() {
        return LOG_CONTEXT;
    }

    const std::string CCommandManager::GetName() {
        return LOG_CONTEXT;
    }

    CATLError CCommandManager::updateAccessManager(TSmartPtr<CAccessManagerConfig> amc) {
        CATLError result;
        if (isAcessManagerConfigured()) {
            am->Close();
            am->Terminate();
        }
        // Access Manager needs an offset of the first debug register to make API transactions
        // the only ways is to get this offset from request via CAccessManagerConfig data
        std::string fw_acc;
        CATLConfig cfg;
        if (SysConfig()->HasOption("fw-acc")) {
            fw_acc = SysConfig()->GetValue("fw-acc");
        } else { // fw access is not specified
            // try to understand what we have in HW
            if (amc->has1K) { // we have 1K memory, will use it for firmware access
                SysLog()->LogDebug(LOG_CONTEXT, "hardware has buffer: fw access is set to hw buffer");
                fw_acc = "hw";
                cfg.SetOption("fw-acc", fw_acc);
            }
        }
        if ((fw_acc.empty() || fw_acc == "direct") && !SysConfig()->HasOption("dbg-offset")) { // we don't have 1K buffer: fw-acc = `` (or don't want to use it: fw-acc = `direct`) and no debug offset specified manually
            if (amc->hasDebugOffset) { // try to get debug offset from request
                SysLog()->LogDebug(LOG_CONTEXT, "using direct access for firmware with debug offset " FSIZE_T, amc->debugOffset);
                cfg.SetOption("dbg-offset", amc->debugOffset);
                cfg.SetOption("fw-acc", "direct");
            } else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: 1st debug register is required but is missing in XML file");
            }
        } else if (fw_acc == "hw") { // we asked to use 1K hw buffer for firmware access
            if (amc->has1K) { // we have 1K buffer in hardware
                if (amc->hwBufOffset >= 0 && amc->hwBufSize > 0x10) { // that should not be set manually from command  line parameters
                    cfg.SetOption("hw-buf-offset", amc->hwBufOffset);
                    cfg.SetOption("hw-buf-size", amc->hwBufSize); // hw buffer usually is 1K
                } else {
                    result = EATLErrorFatal;
                    SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: offest " FSIZE_T " and size " FSIZE_T " for hw buffer are incorrect in request", amc->hwBufOffset, amc->hwBufSize);
                }
            } else {
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: no 1K hw buffer is avaiable");
            }
        }
        if (result.IsOk()) {
            SysConfig()->Override(cfg); // apply gathered parameters to current config
            am = new CAccessManager();
            result = am->Initialize(); // init new access manager
            if (result.IsOk()) {
                result = am->Open(); // open it
                if (result.IsError()) {
                    SysLog()->LogError(LOG_CONTEXT, "failed to open access manager");
                } else {
                    SysLog()->LogInfo(LOG_CONTEXT, "access manager configured successfully");
                    SysLog()->LogInfo(LOG_CONTEXT, "hw access type: %s", am->GetHWAccessType().c_str());
                    SysLog()->LogInfo(LOG_CONTEXT, "fw access type: %s", am->GetFWAccessType().c_str());
                }
            }
        }
        return result;
    }

    CATLError CCommandManager::Initialize(const flags32&) {
        CATLError result;
        documentRoot = SysConfig()->GetValue("http-root");
        if ( isXMLLoaded() ) {
            SysLog()->LogDebug(LOG_CONTEXT, "file ApicalControl.xml has been loaded");
        } else {
            SysLog()->LogInfo(LOG_CONTEXT, "file ApicalControl.xml must be loaded via front-end");
        }
        return result;
    }

    CATLError CCommandManager::Open() { // we init and open AM at first request: nothing to do here
        return EATLErrorOk;
    }

    CATLError CCommandManager::Close() {
        CATLError result;
        if (isAcessManagerConfigured()) {
            result = am->Close();
        }
        return result;
    }

    CATLError CCommandManager::Terminate() {
        CATLError result;
        if (isAcessManagerConfigured()) {
            result = am->Terminate();
        }
        return result;
    }

    std::pair< CATLError, TSmartPtr <CRegRequest> > CCommandManager::sendRegReadReq (const std::string& type, const baseaddr &offset, const basesize &size) {

        TSmartPtr<CRegRequest> request = new CRegRequest();
        if ( type == "r" ) { // isp register
            request->type = RegRead;
        } else if ( type == "a" ) { // isp array
            request->type = RegRead;
        } else if ( type == "l" ) { // isp lut (deprecated)
            request->type = LUTRead;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "unsupported ISP register type `%s`", type.c_str() );
            return std::pair< CATLError, TSmartPtr < CRegRequest > > (EATLErrorUnsupported , request);
        }
        request->address = offset;
        request->data.resize( size, 0 );

        CATLError result = am->PostRegRequest( request ); // send async request to hardware
        return std::pair< CATLError, TSmartPtr < CRegRequest > > (result , request);

    }

    std::pair< CATLError, TSmartPtr <CRegRequest> > CCommandManager::sendRegWriteReq (const std::string& type, const baseaddr& offset, const std::vector<UInt8>& value, const std::vector<UInt8>& mask) {
        basesize sz = value.size();
        TSmartPtr<CRegRequest> request = new CRegRequest();
        if ( type == "r" ) { // isp register
            request->type = RegWrite;
            request->mask.resize(sz, 0);
            for (basesize i = 0; i < sz; i++) {
                request->mask[i] = mask[i];
            }
        } else if ( type == "a" ) { // isp array
            request->mask.resize(0);
            request->type = RegWrite;
        } else if ( type == "l" ) { // isp lut (deprecated)
            request->mask.resize(0);
            request->type = LUTWrite;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "unsupported ISP register type `%s`", type.c_str() );
            return std::pair< CATLError, TSmartPtr < CRegRequest > > (EATLErrorUnsupported , request);
        }
        request->address = offset;
        request->data.resize(sz, 0);
        for (basesize i = 0; i < sz; i++) {
            request->data[i] = value[i];
        }
        CATLError result = am->PostRegRequest( request );
        return std::pair< CATLError, TSmartPtr < CRegRequest > > (result , request);

    }

    std::pair< CATLError, TSmartPtr <CFWRequest> > CCommandManager::sendApiReq(const EFWRequestType& t, const UInt8& s, const UInt8& c, const UInt32& p) {
        TSmartPtr<CFWRequest> request = new CFWRequest();
        request->type = t;
        request->cmd_type = s;
        request->cmd = c;
        request->value = p;
        CATLError result = am->PostFWRequest( request );
        return std::pair< CATLError, TSmartPtr < CFWRequest > > (result , request);
    }

    CATLError CCommandManager::processRegReq (TSmartPtr<CRegRequest> req, TSmartPtr< CResponseWriter > answer, const std::string& key) {
        TSmartPtr<CRegRequest> request = am->GetRegResult( req->id, 0 );
        switch (request->state) {
            case Fail:
                SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected");
                return EATLErrorDeviceError;
            case Timeout:
                SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded");
                return EATLErrorTimeout;
            case Success:
                answer->AddDataArray( key, request->data );
                return EATLErrorOk;
            case NoAnswer:
                SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device");
                return EATLErrorDeviceError;
            default:
                SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error");
                return EATLErrorUnsupported;
        }
    }

    CATLError CCommandManager::processApiReq (TSmartPtr<CFWRequest> req, TSmartPtr< CResponseWriter > answer, const std::string& key) {
        TSmartPtr<CFWRequest> request = am->GetFWResult( req->id, 0 );
        switch (request->state) {
            case Fail:
                SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected" );
                return EATLErrorDeviceError;
            case Timeout:
                SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded" );
                return EATLErrorTimeout;
            case Success:
                answer->AddField( key, request->value );
                return EATLErrorOk;
            case NoAnswer:
                SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device" );
                return EATLErrorDeviceError;
            default:
                SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error" );
                return EATLErrorUnsupported;
        }
    }

    CATLError CCommandManager::CommandRegRead( const TSmartPtr< CRequestParser >& clientRequest, TSmartPtr< CResponseWriter > answer ) {

        std::string reg_type = clientRequest->GetType();
        baseaddr reg_offset = clientRequest->GetOffset();
        basesize reg_size = clientRequest->GetSize();

        SysLog()->LogDebug(LOG_CONTEXT, "reg read {type: %s, offset: %ld, size: " FSIZE_T "}", reg_type.c_str(), reg_offset, reg_size);

        if (!clientRequest->isValid()) {
            return EATLErrorInvalidParameters;
        }

        std::pair< CATLError, TSmartPtr < CRegRequest > > data = sendRegReadReq(reg_type, reg_offset, reg_size);
        if ( data.first.IsOk() ) { // wait for response from access manager
            return processRegReq(data.second, answer);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "read request has not been accepted by access manager" );
            return data.first;
        }

    }

    CATLError CCommandManager::CommandRegPoll(const TSmartPtr< CRequestParser >& clientRequest, TSmartPtr< CResponseWriter > answer ) {

        std::vector< TSmartPtr < CRegPollCommand > > commands = clientRequest->GetRegPollCommands();
        basesize asize = commands.size();
        if (!clientRequest->isValid() || asize == 0) {
            return EATLErrorInvalidParameters;
        }

        std::vector < TSmartPtr < CRegRequest > > requests;
        std::vector < basesize > indices;
        for(basesize c = 0; c < asize; c++) {
            std::pair< CATLError, TSmartPtr < CRegRequest > > data = sendRegReadReq(commands[c]->type, commands[c]->offset, commands[c]->size);
            if ( data.first.IsOk() ) {
                requests.push_back(data.second);
                indices.push_back(c);
            }
        }

        CATLError res;
        basesize xc;
        basesize successes = 0;
        basesize cnt = requests.size();
        for ( basesize idx = 0; idx < cnt; idx ++ ) {
            xc = indices.at(idx);
            res = processRegReq(requests.at(idx), answer, commands[xc]->name);
            if (res == EATLErrorOk) {
                successes++;
            }
        }

        if ( asize == successes ) {
            return EATLErrorOk;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "something went wrong: not all hw polling requests got response" );
            return EATLErrorUnsupported;
        }

    }

    CATLError CCommandManager::CommandRegWrite( const TSmartPtr< CRequestParser >& clientRequest, TSmartPtr< CResponseWriter > answer ) {

        std::string reg_type = clientRequest->GetType();
        baseaddr reg_offset = clientRequest->GetOffset();

        std::vector<UInt8> data = clientRequest->GetData();

        SysLog()->LogDebug(LOG_CONTEXT, "reg write {type: %s, offset: %ld, size: " FSIZE_T "}", reg_type.c_str(), reg_offset, data.size());

        std::vector<UInt8> mask;
        if (reg_type == "r") {
            mask = clientRequest->GetMask();
            basesize vlen = data.size();
            basesize mlen = mask.size();
            if (vlen != mlen) {
                SysLog()->LogError(LOG_CONTEXT, "data length is not equal to mask length" );
                return EATLErrorInvalidParameters;
            }
        } else {
            mask.resize(0);
        }

        if (!clientRequest->isValid()) {
            SysLog()->LogError(LOG_CONTEXT, "some fields are missing in request" );
            return EATLErrorInvalidParameters;
        }

        std::pair< CATLError, TSmartPtr < CRegRequest > > rq = sendRegWriteReq(reg_type, reg_offset, data, mask);
        if ( rq.first.IsOk() ) { // wait for response from access manager
            return processRegReq(rq.second, answer);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "write request has not been accepted by access manager" );
            answer->AddField("details", "device unable to accept the request");
            return rq.first;
        }

    }

    CATLError CCommandManager::CommandApiRead( const TSmartPtr< CRequestParser >& clientRequest, TSmartPtr< CResponseWriter > answer ) {

        UInt8 section = clientRequest->GetSection();
        UInt8 command = clientRequest->GetCommand();
        UInt32 page = clientRequest->GetPage(); // if not present - use default value 0

        SysLog()->LogDebug(LOG_CONTEXT, "api read {section: %d, command: %d, page: %d}", (UInt16)section, (UInt16)command, page);

        if (!clientRequest->isValid()) {
            SysLog()->LogError(LOG_CONTEXT, "request is incomplete" );
            return EATLErrorInvalidParameters;
        }
        std::pair< CATLError, TSmartPtr < CFWRequest > > data = sendApiReq(APIRead, section, command, page);
        if ( data.first.IsOk() ) { // wait until result is ready
            return processApiReq(data.second, answer);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager" );
            return data.first;
        }

    }

    CATLError CCommandManager::CommandApiPoll( const TSmartPtr< CRequestParser > & clientRequest, TSmartPtr< CResponseWriter > answer ) {

        std::vector< TSmartPtr < CApiPollCommand > > commands = clientRequest->GetApiPollCommands();
        basesize asize = commands.size();
        if (!clientRequest->isValid() || asize == 0) {
            return EATLErrorInvalidParameters;
        }
        std::vector < TSmartPtr < CFWRequest > > requests;
        std::vector < basesize > indices;
        for(basesize c = 0; c < asize; c++) {
            std::pair< CATLError, TSmartPtr < CFWRequest > > data = sendApiReq(APIRead, commands[c]->section, commands[c]->command, 0);
            if ( data.first.IsOk() ) {
                requests.push_back(data.second);
                indices.push_back(c);
            }
        }

        CATLError res;
        basesize xc;
        basesize successes = 0;
        basesize cnt = requests.size();
        for ( basesize idx = 0; idx < cnt; idx ++ ) {
            xc = indices.at(idx);
            res = processApiReq(requests.at(idx), answer, commands[xc]->name);
            if (res == EATLErrorOk) {
                successes++;
            }
        }

        if ( asize == successes ) {
            return EATLErrorOk;
        } else {
            SysLog()->LogError(LOG_CONTEXT, "something went wrong: not all fw polling requests got response" );
            return EATLErrorUnsupported;
        }

    }

    CATLError CCommandManager::CommandApiWrite( const TSmartPtr< CRequestParser > & clientRequest, TSmartPtr< CResponseWriter > answer ) {

        UInt8 section = clientRequest->GetSection();
        UInt8 command = clientRequest->GetCommand();
        UInt32 page = clientRequest->GetPage(true); // page must be sent in request

        SysLog()->LogDebug(LOG_CONTEXT, "api write {section: %d, command: %d, value: %d}", (UInt16)section, (UInt16)command, page);

        if (!clientRequest->isValid()) {
            SysLog()->LogError(LOG_CONTEXT, "request is incomplete" );
            return EATLErrorInvalidParameters;
        }
        std::pair< CATLError, TSmartPtr < CFWRequest > > data = sendApiReq(APIWrite, section, command, page);
        if ( data.first.IsOk() ) { // wait until result is ready
            return processApiReq(data.second, answer);
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager" );
            return data.first;
        }

    }

    CATLError CCommandManager::CommandLutRead( const TSmartPtr< CRequestParser > & clientRequest, TSmartPtr< CResponseWriter > answer ) {

        UInt32 page = clientRequest->GetPage(true); // page must be sent in request
        basesize sz = clientRequest->GetSize();

        SysLog()->LogDebug(LOG_CONTEXT, "calibration read {page: %d, type: 0, size: " FSIZE_T "}", page, sz);

        if (sz == 0 || !clientRequest->isValid()) {
            SysLog()->LogError(LOG_CONTEXT, "request is incomplete" );
            return EATLErrorInvalidParameters;
        }

        TSmartPtr<CFWRequest> request = new CFWRequest();
        request->type = BUFRead;
        request->cmd_type = 0;
        request->cmd = page;
        request->data.resize(sz, 0);

        CATLError result = am->PostFWRequest( request );

        if ( result.IsOk() ) { // wait until result is ready
            request = am->GetFWResult( request->id, 0 );
            switch (request->state) {
                case Fail:
                    SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected" );
                    return EATLErrorDeviceError;
                case Timeout:
                    SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded" );
                    return EATLErrorTimeout;
                case Success: {
                    answer->AddDataArray( "val", request->data );
                    return EATLErrorOk;
                }
                case NoAnswer:
                    SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device" );
                    return EATLErrorDeviceError;
                case Processing:
                default:
                    SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error" );
                    return EATLErrorUnsupported;
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager" );
            return result;
        }

    }

    CATLError CCommandManager::CommandLutWrite( const TSmartPtr< CRequestParser > & clientRequest, TSmartPtr< CResponseWriter > ) {

        UInt32 page = clientRequest->GetPage(true); // page must be sent in request
        std::vector< UInt8 > data = clientRequest->GetData();
        basesize sz = data.size();

        SysLog()->LogDebug(LOG_CONTEXT, "calibration write {page: %d, type: 0, size: " FSIZE_T "}", page, sz);

        if (!clientRequest->isValid() || sz == 0) {
            SysLog()->LogError(LOG_CONTEXT, "request is incomplete" );
            return EATLErrorInvalidParameters;
        }

        TSmartPtr<CFWRequest> request = new CFWRequest();
        request->type = BUFWrite;
        request->cmd_type = 0;
        request->cmd = page;
        request->data.resize(sz);
        for (basesize i = 0; i < sz; i++) {
            request->data[i] = data[i];
        }

        CATLError result = am->PostFWRequest( request );

        if ( result.IsOk() ) { // wait until result is ready
            request = am->GetFWResult( request->id, 0 );
            switch (request->state) {
            case Fail:
                SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected" );
                return EATLErrorDeviceError;
            case Timeout:
                SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded" );
                return EATLErrorTimeout;
            case Success:
                return EATLErrorOk;
            case NoAnswer:
                SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device" );
                return EATLErrorDeviceError;
            default:
                SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error" );
                return EATLErrorUnsupported;
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager" );
            return result;
        }
    }

    basebool CCommandManager::isXMLLoaded() const {
        std::ifstream f((documentRoot + FILE_SEPARATOR + "ApicalControl.xml").c_str());
        if (f.good()) {
            f.close();
            return true;
        } else {
            f.close();
            return false;
        }
    }

    CATLError CCommandManager::saveXml(const std::string& text) {

        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        CATLError result = EATLErrorOk;

        std::ofstream xmlFile;
        std::string xmlFileName = documentRoot + FILE_SEPARATOR + "ApicalControl.xml";
        xmlFile.open (xmlFileName.c_str());
        if (xmlFile.is_open() && xmlFile) {
            xmlFile << text;
            SysLog()->LogInfo(LOG_CONTEXT, "new XML file has been successfully loaded: " FSIZE_T " bytes", text.size());
            if (isAcessManagerConfigured()) {
                am->Close();
                am->Terminate();
            }
            result |= dropApi();
        } else {
            result = EATLErrorNoFreeSpace;
            SysLog()->LogError(LOG_CONTEXT, "new XML file can not be written to directory `%s`: check if destination is writable", documentRoot.c_str() );
        }
        xmlFile.close();

        return result;
    }

    CATLError CCommandManager::saveApi(const std::string& text) {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        CATLError result = EATLErrorOk;
        std::ofstream apiFile;
        std::string apiFileName = documentRoot + FILE_SEPARATOR + "command.json";
        apiFile.open (apiFileName.c_str());
        if (apiFile.is_open() && apiFile) {
            apiFile << text;
            SysLog()->LogInfo(LOG_CONTEXT, "new API file has been successfully loaded: " FSIZE_T " bytes", text.size());
            if (isAcessManagerConfigured()) {
                am->Close();
                am->Terminate();
            }
        } else {
            result = EATLErrorOk;
            SysLog()->LogError(LOG_CONTEXT, "new API file can not be written to directory `%s`: check if destination is writable", documentRoot.c_str() );
        }
        apiFile.close();
        return result;
    }

    CATLError CCommandManager::dropApi() {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        std::string fpath = documentRoot + FILE_SEPARATOR + "command.json";
        std::ifstream f(fpath.c_str());
        if (f.good()) {
            f.close();
            if (std::remove(fpath.c_str())) {
                SysLog()->LogError(LOG_CONTEXT, "can't remove API file" );
                return EATLErrorFatal;
            } else {
                SysLog()->LogInfo(LOG_CONTEXT, "API file removed from server" );
                return EATLErrorOk;
            }
        } else {
            f.close();
            return EATLErrorOk;
        }
    }

    CATLError CCommandManager::configAccessManager(const std::string &text) {
        armcpp11::LockGuard<armcpp11::RecursiveMutex> lock( sync );
        if (isAcessManagerConfigured()) {
            return EATLErrorOk;
        } else {
            CATLError result =  EATLErrorOk;
            TSmartPtr< CRequestParser > request = new CRequestParser( text );
            TSmartPtr< CAccessManagerConfig > params = request->GetAccessManagerConfig();
            if (params->initialized) {
                result = updateAccessManager(params);
            } else {
                result = EATLErrorInvalidParameters;
                SysLog()->LogError(LOG_CONTEXT, "no config for access manager has been provided in request");
            }
            return result;
        }
    }

    const std::pair< std::string, UInt16 > CCommandManager::getErrorResponse (const CATLError& result) {
        switch (result.GetErrorCode()) {
            case EATLErrorNotChanged:           return std::pair <std::string, UInt16> ("Not changed", 202);
            case EATLErrorInvalidValue:         return std::pair <std::string, UInt16> ("Bad data in request", 400);
            case EATLErrorInvalidParameters:    return std::pair <std::string, UInt16> ("Invalid parameters in request", 400);
            case EATLErrorUnsupported:          return std::pair <std::string, UInt16> ("Not supported", 400);
            case EATLErrorFatal:                return std::pair <std::string, UInt16> ("Server fatal error", 520);
            case EATLErrorNotImplemented:       return std::pair <std::string, UInt16> ("Not implemented", 501);
            case EATLErrorNoMoreData:           return std::pair <std::string, UInt16> ("No more data available", 502);
            case EATLErrorNoEnoughBuffer:       return std::pair <std::string, UInt16> ("Buffer too small to process the request", 502);
            case EATLErrorDeviceError:          return std::pair <std::string, UInt16> ("Device error", 502);
            case EATLErrorNotInitialized:       return std::pair <std::string, UInt16> ("Server or device are not initialized", 503);
            case EATLErrorTimeout:              return std::pair <std::string, UInt16> ("Request to device timed out", 504);
            case EATLErrorNoMemory:             return std::pair <std::string, UInt16> ("Not enough memeory on server", 507);
            case EATLErrorNoFreeSpace:          return std::pair <std::string, UInt16> ("No free space", 507);
            default:                            return std::pair <std::string, UInt16> ("Unknown error", 520);
        }
    }

    const std::pair< std::string, UInt16 > CCommandManager::proxy(const std::string& text, const UInt8& route) {
        TSmartPtr< CRequestParser > request = new CRequestParser( text );
        if ( request->isValid() ) {
            TSmartPtr< CResponseWriter > reply = new CResponseWriter();
            reply->StartWriting();
            CATLError result;
            switch (route) {
            case CCommandManager::HW_READ:
                result = CommandRegRead(request, reply);
                break;
            case CCommandManager::HW_POLL:
                result = CommandRegPoll(request, reply);
                break;
            case CCommandManager::HW_WRITE:
                result = CommandRegWrite(request, reply);
                break;
            case CCommandManager::FW_GET:
                result = CommandApiRead(request, reply);
                break;
            case CCommandManager::FW_POLL:
                result = CommandApiPoll(request, reply);
                break;
            case CCommandManager::FW_SET:
                result = CommandApiWrite(request, reply);
                break;
            case CCommandManager::CA_LOAD:
                result = CommandLutRead(request, reply);
                break;
            case CCommandManager::CA_SAVE:
                result = CommandLutWrite(request, reply);
                break;
            default:
                result = EATLErrorNotImplemented;
                break;
            }
            reply->FinishWriting();
            if (result.IsOk()) {
                return std::pair < std::string, UInt16 > ( reply->serialize(), 200 );
            } else {
                return getErrorResponse(result);
            }
        } else {
            return std::pair < std::string, UInt16 > ( "Invalid request syntax", 400 );
        }
    }

    const std::pair<std::string, UInt16> CCommandManager::downloadFile(const UInt8& sec, const UInt8& cmd) {

        TSmartPtr<CFWRequest> request = new CFWRequest();
        request->type = BUFRead;
        request->cmd_type = sec;
        request->cmd = cmd;
        request->data.resize(0);

        CATLError result = am->PostFWRequest(request);

        if ( result.IsOk() ) { // wait until result is ready
            request = am->GetFWResult( request->id, 0 );
            switch (request->state) {
                case Fail:
                    SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected" );
                    return getErrorResponse(EATLErrorDeviceError);
                case Timeout:
                    SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded" );
                    return getErrorResponse(EATLErrorTimeout);
                case Success: {
                    TSmartPtr< CResponseWriter > reply = new CResponseWriter();
                    reply->StartWriting();
                    reply->AddDataArray("file", request->data);
                    reply->FinishWriting();
                    return std::pair <std::string, UInt16> (reply->serialize(), 200);
                }
                case NoAnswer:
                    SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device" );
                    return getErrorResponse(EATLErrorDeviceError);
                case Processing:
                default:
                    SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error" );
                    return getErrorResponse(EATLErrorUnsupported);
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager" );
            return getErrorResponse(result);
        }

    }

    const std::pair< std::string, UInt16 > CCommandManager::uploadFile(const std::vector<UInt8>& data, const UInt8& sec, const UInt8& cmd) {

        basesize sz = data.size();

        TSmartPtr<CFWRequest> request = new CFWRequest();
        request->type = BUFWrite;
        request->cmd_type = sec;
        request->cmd = cmd;
        request->data.resize(sz);
        for (basesize i = 0; i < sz; i++) {
            request->data[i] = data[i];
        }

        CATLError result = am->PostFWRequest(request);

        if (result.IsOk()) { // wait until result is ready
            request = am->GetFWResult(request->id, 0);
            switch (request->state) {
            case Fail:
                SysLog()->LogError(LOG_CONTEXT, "probably device is disconnected");
                return getErrorResponse(EATLErrorDeviceError);
            case Timeout:
                SysLog()->LogError(LOG_CONTEXT, "device timeout exceeded");
                return getErrorResponse(EATLErrorTimeout);
            case Success:
                return getErrorResponse(EATLErrorOk);
            case NoAnswer:
                SysLog()->LogError(LOG_CONTEXT, "access manager has not received response from device");
                return getErrorResponse(EATLErrorDeviceError);
            default:
                SysLog()->LogError(LOG_CONTEXT, "access manager returned unknown error");
                return getErrorResponse(EATLErrorUnsupported);
            }
        } else {
            SysLog()->LogError(LOG_CONTEXT, "request has not been accepted by access manager");
            return getErrorResponse(result);
        }

    }

    basebool CCommandManager::isAcessManagerConfigured() const {
        if (am == NULL) {
            return false;
        } else {
            return am->isConfigured();
        }
    }

    const std::string CCommandManager::hwAccessType() const {
        if (am == NULL) {
            return "unset";
        } else {
            return am->GetHWAccessType();
        }
    }

    const std::string CCommandManager::fwAccessType() const {
        if (am == NULL) {
            return "unset";
        } else {
            return am->GetFWAccessType();
        }
    }

}

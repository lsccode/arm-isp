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

#ifdef ACT_SUPPORT_DIRECT_COMMAND

#define LOG_CONTEXT "ACTDirect"

#include "ACTDirectCommand.h"

#include <map>
#include <sstream>
#include <iomanip>

using namespace act;

static std::map<std::string, std::string> defaults;

CACTDirectCommand::CACTDirectCommand() {
    SysConfig()->Merge(defaults);
}

CATLError CACTDirectCommand::CheckForImmediateAction() {
    return EATLErrorOk;
}

CATLError CACTDirectCommand::Execute() {
    CATLError result;
    SysLog()->LogDebug(LOG_CONTEXT, "start execution...");


    if (isAcessManagerConfigured()) {
        am->Close();
        am->Terminate();
    }

    std::string fw_acc;
    if (SysConfig()->HasOption("fw-acc")) {
        fw_acc = SysConfig()->GetValue("fw-acc");
    } else { // fw access is not specified
        // try to understand what we have in HW
        if (SysConfig()->HasOption("hw-buf-offset") && SysConfig()->HasOption("hw-buf-size")) { // we have 1K memory, will use it for firmware access
            SysLog()->LogDebug(LOG_CONTEXT, "hardware has buffer: fw access is set to hw buffer");
            fw_acc = "hw";
            SysConfig()->SetOption("fw-acc", fw_acc);
        }
    }
    if ((fw_acc.empty() || fw_acc == "direct") && !SysConfig()->HasOption("dbg-offset")) { // we don't have 1K buffer: fw-acc = `` (or don't want to use it: fw-acc = `direct`) and no debug offset specified
        result = EATLErrorFatal;
        SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: 1st debug register is required but is missing in command line parameters");
    } else if (fw_acc == "hw") { // we asked to use 1K hw buffer for firmware access
        if (SysConfig()->HasOption("hw-buf-offset") && SysConfig()->HasOption("hw-buf-size")) { // we have 1K buffer in hardware
            baseaddr hwBufferOffset = SysConfig()->GetValueAsU32("hw-buf-offset");
            UInt32 hwBufferSize = SysConfig()->GetValueAsU32("hw-buf-size");
            if (hwBufferOffset < 0 && hwBufferSize < 0x10) { // that should not be set manually from command line parameters
                result = EATLErrorFatal;
                SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: offest " FSIZE_T " and size " FSIZE_T " for hw buffer are incorrect in request", hwBufferOffset, hwBufferSize);
            }
        } else {
            result = EATLErrorFatal;
            SysLog()->LogError(LOG_CONTEXT, "failed to initialize access manager: no 1K hw buffer is avaiable");
        }
    }
    if (result.IsOk()) {
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

    static std::string greeting = "command (h for help): ";
    static std::string hex_prefix = "0x";

    std::string cmd;
    std::cout << greeting;
    while (std::getline(std::cin, cmd)) { // main user interaction loop: this will exit on Ctrl+D (EOF) as well
        if (cmd == "q" || cmd == "quit" || cmd == "exit") { // valid exit commands
            break;
        }
        if (cmd == "h" || cmd == "help") { // show help
            std::cout << " * general commands:" << std::endl;
            std::cout << "   - h - print help message" << std::endl;
            std::cout << "   - q - quit from application" << std::endl;
            std::cout << " * hw r {offset} {size} - read bytes from hardware at <offset>:" << std::endl;
            std::cout << " * hw w {offset} {bytes} - write bytes to hardware at <offset>:" << std::endl;
            std::cout << "   - offset  - address to read from: dec or hex (0x prefix) 32-bit number" << std::endl;
            std::cout << "   - size   - number of bytes to read: dec or hex (0x prefix) 32-bit number" << std::endl;
            std::cout << "   - bytes   - hex low-case digits: e.g. 01aaff22" << std::endl;
            std::cout << " * api r {sec} {cmd} - read from firmware api" << std::endl;
            std::cout << " * api w {sec} {cmd} {value} - write value to firmware api" << std::endl;
            std::cout << "   - sec     - section: 8-bit number" << std::endl;
            std::cout << "   - cmd     - command: 8-bit number" << std::endl;
            std::cout << "   - value   - value to set: 32-bit number" << std::endl;
        }
        std::stringstream stream;
        stream << cmd;
        std::string dev, dir;
        stream >> dev >> dir;
        if (dev == "hw") { // hw command
            std::string data;
            UInt32 offset;
            stream >> offset >> data;
            if (dir == "r" && offset >= 0 && data.size() > 0) { // read command
                TSmartPtr<CRegRequest> hr = new CRegRequest();
                hr->type = RegRead;
                hr->address = offset;
                UInt32 size = stou32(data);
                hr->data.resize(size, 0);
                CATLError r = am->PostRegRequest(hr);
                if (r.IsOk()) {
                    hr = am->GetRegResult(hr->id, 0);
                    if (hr->state == Success) {
                        std::cout << "response (" << hr->data.size() << " byte(s)): ";
                        for (std::vector<UInt8>::const_iterator it = hr->data.begin();
                             it != hr->data.end(); ++it) {
//                        for (UInt8 byte : hr->data) {
                            std::cout << std::setw(2) << std::setfill('0') << std::hex << (UInt16)*it/*byte*/;
                        }
                        std::cout << std::endl;
                    }
                }
            } else if (dir == "w" && offset >= 0 && data.size() > 0 && (data.size() % 2) == 0) { // write command
                TSmartPtr<CRegRequest> hr = new CRegRequest();
                hr->type = RegWrite;
                hr->address = offset;
                basesize size = data.size() / 2;
                hr->data.resize(size, 0);
                for (basesize x = 0; x < size; x++) {
                    hr->data[x] = stou8(hex_prefix + data.at(2*x) + data.at(2*x + 1));
                }
                CATLError r = am->PostRegRequest(hr);
                if (r.IsOk()) {
                    hr = am->GetRegResult(hr->id, 0);
                    if (hr->state == Success) {
                        std::cout << "success " << hr->data.size() << " byte(s) written" << std::endl;
                    }
                }
            } else {
                std::cout << "wrong command syntax (enter h for reference)" << std::endl;
            }
        } else if (dev == "api") { // firmware api commands
            std::string section;
            std::string command;
            stream >> section >> command;
            if (dir == "r" && section.size() > 0 && command.size() > 0) { // read command
                TSmartPtr<CFWRequest> fr = new CFWRequest();
                fr->type = APIRead;
                fr->cmd_type = stou8(section);
                fr->cmd = stou8(command);
                fr->value = 0;
                CATLError r = am->PostFWRequest(fr);
                if (r.IsOk()) {
                    fr = am->GetFWResult(fr->id, 0);
                    if (fr->state == Success) {
                        std::cout << "response: " << fr->value << std::endl;
                    } else {
                        std::cout << "error - state: " << (int)fr->state << std::endl;
                    }
                } else {
                    std::cout << "error: " << r.GetErrorCode() << std::endl;
                }
            } else if (dir == "w" && section.size() > 0 && command.size() > 0) {
                std::string value;
                stream >> value;
                TSmartPtr<CFWRequest> fr = new CFWRequest();
                fr->type = APIWrite;
                fr->cmd_type = stou8(section);
                fr->cmd = stou8(command);
                fr->value = stou32(value);
                CATLError r = am->PostFWRequest(fr);
                if (r.IsOk()) {
                    fr = am->GetFWResult(fr->id, 0);
                    if (fr->state == Success) {
                        std::cout << "success"<< std::endl;
                    } else {
                        std::cout << "error - state: " << (int)fr->state << std::endl;
                    }
                } else {
                    std::cout << "error: " << r.GetErrorCode() << std::endl;
                }
            } else {
                std::cout << "wrong command syntax (enter h for reference)" << std::endl;
            }
        } else {
            std::cout << "wrong command syntax (enter h for reference)" << std::endl;
        }
        std::cout << greeting;
    }

    Stop();

    SysLog()->LogDebug(LOG_CONTEXT, "finished execution");
    return result;
}

CATLError CACTDirectCommand::Stop() {
    CATLError res;
    if (isAcessManagerConfigured()) {
        res = am->Close();
        if (res.IsOk()) {
            res = am->Terminate();
        }
    }
    return res;
}

void CACTDirectCommand::ShowUsage() {
    std::cout << "" << std::endl;
}

basebool CACTDirectCommand::isAcessManagerConfigured() const {
    if (am == NULL) {
        return false;
    } else {
        return am->isConfigured();
    }
}

#endif // ACT_SUPPORT_DIRECT_COMMAND


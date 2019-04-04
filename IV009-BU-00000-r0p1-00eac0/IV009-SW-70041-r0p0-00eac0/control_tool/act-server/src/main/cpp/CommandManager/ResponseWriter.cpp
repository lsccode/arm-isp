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

#include <ATL/ATLLogger.h>

#include <CommandManager/ResponseWriter.h>

#define LOG_CONTEXT "ResponseWriter"

namespace act {

    const std::string CResponseWriter::comma  = std::string(",");
    const std::string CResponseWriter::quote  = std::string("\"");
    const std::string CResponseWriter::lbrace = std::string("{");
    const std::string CResponseWriter::rbrace = std::string("}");
    const std::string CResponseWriter::colon  = std::string(":");
    const std::string CResponseWriter::eqstr  = std::string("\":\"");
    const std::string CResponseWriter::eqnum  = std::string("\":");
    const std::string CResponseWriter::eqarr  = std::string("\":[");
    const std::string CResponseWriter::endarr = std::string("]");

    CResponseWriter::CResponseWriter() {
        started = false;
        firstField = false;
        keys.clear();
        text.clear();
    }

    const std::string CResponseWriter::GetObjectStaticName() {
        return LOG_CONTEXT ;
    }

    CResponseWriter::~CResponseWriter() {}

    const std::string CResponseWriter::serialize() const {
        std::string data;
        data.assign(text);
        return data;
    }

    atl::CATLError CResponseWriter::StartWriting() {
        started = true;
        firstField = true;
        keys.clear();
        text.clear();
        text += lbrace;
        return atl::EATLErrorOk;
    }

    atl::CATLError CResponseWriter::FinishWriting() {
        if (started) {
            text += rbrace;
            keys.clear();
            started = false;
            return atl::EATLErrorOk;
        } else {
            started = false;
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to close empty response");
            return atl::EATLErrorNotInitialized;
        }
    }

    void CResponseWriter::addComma() {
        if (firstField)  {
            firstField = false;
        } else {
            text += comma;
        }
    }

    atl::CATLError CResponseWriter::AddField(const std::string& name, const std::string& value) {
        if (keys.find(name) != keys.end()) {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to add duplicated field %s", name.c_str());
            return atl::EATLErrorInvalidParameters;
        }
        if (started) {
            addComma();
            text += (quote + name + eqstr + value + quote);
            keys.insert(name);
            return atl::EATLErrorOk;
        } else {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to append data to uninitialized response");
            return atl::EATLErrorNotInitialized;
        }
    }

    atl::CATLError CResponseWriter::AddField(const std::string& name, const atl::UInt32& value) {
        if (keys.find(name) != keys.end()) {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to add duplicated field %s", name.c_str());
            return atl::EATLErrorInvalidParameters;
        }
        if (started) {
            addComma();
            text += (quote + name + eqnum + atl::utos(value));
            keys.insert(name);
            return atl::EATLErrorOk;
        } else {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to append data to uninitialized response");
            return atl::EATLErrorNotInitialized;
        }
    }

    atl::CATLError CResponseWriter::AddDataArray(const std::string& name, std::vector<atl::UInt8>& values) {
        if (keys.find(name) != keys.end()) {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to add duplicated field %s", name.c_str());
            return atl::EATLErrorInvalidParameters;
        }
        if (started) {
            addComma();
            text += (quote + name + eqarr);
            if (values.size() > 0) {
                atl::basesize asize = values.size() - 1;
                for (atl::UInt32 i = 0; i <= asize; i++) {
                    text += (atl::utos(values.at(i)));
                    if (i != asize) {
                        text += comma;
                    }
                }
            }
            text += endarr;
            keys.insert(name);
            return atl::EATLErrorOk;
        } else {
            atl::SysLog()->LogError(LOG_CONTEXT, "attempt to append data to uninitialized response");
            return atl::EATLErrorNotInitialized;
        }
    }

}


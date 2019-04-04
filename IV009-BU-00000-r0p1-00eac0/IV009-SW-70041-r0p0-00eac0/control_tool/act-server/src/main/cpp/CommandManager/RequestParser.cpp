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

#include <CommandManager/RequestParser.h>

#define LOG_CONTEXT "RequestParser"

namespace act {

    CRequestParser::CRequestParser(const std::string &text) {
        types.clear();
        values.clear();
        valid = parse(text);
        if (!valid) {
            types.clear();
            values.clear();
        }
    }

    const std::string CRequestParser::GetObjectStaticName() {
        return LOG_CONTEXT ;
    }

    CRequestParser::~CRequestParser() {
    }

    atl::basebool CRequestParser::parse(const std::string &text) {

        std::string::size_type len = text.size();
        if (len < 5) {
            return false;
        }
        char c0 = text[0];
        if (c0 != '"') {
            return false;
        }
        char cn;
        std::string key;
        std::string value;
        bool inKey = true, inValue = false;
        unsigned char type = 0xFF;
        std::string::size_type i;
        for(i = 1; i < len; ++i) {
            cn = text[i];
            if (cn == '\\') {
                i++;
                if (i >= len) {
                    return false;
                }
                cn = text[i];
                if (inKey) {
                    key += cn;
                } else if (inValue) {
                    value += cn;
                } else {
                    return false;
                }
                i++;
                if (i >= len) {
                    return false;
                }
                cn = text[i];
            }
            if (inKey) {
                if (cn == '"') {
                    inKey = false;
                } else {
                    key += cn;
                }
            } else if (inValue) {
                if (cn == '"' && type == 0) { // string scalar completed
                    values[key] = value;
                    key.clear();
                    value.clear();
                    inValue = false;
                    type = 0xFF;
                    continue;
                } else if (cn == '"' && type == 3) { // array of strings
                    value += cn;
                } else if (cn == ';' && (type == 2 || type == 3)) { // number or scalar completed
                    values[key] = value;
                    key.clear();
                    value.clear();
                    inValue = false;
                    type = 0xFF;
                    i++;
                    if (i >= len) {
                        break;
                    }
                    cn = text[i];
                    if (cn == '"') {
                        inKey = true;
                    } else {
                        return false;
                    }
                } else {
                    value += cn;
                }
            } else {
                if (cn == ':') {
                    i++;
                    if (i >= len) {
                        return false;
                    }
                    cn = text[i];
                    if (cn == '"') {
                        type = 0;
                        types[key] = 0; // string
                        inValue = true;
                    } else if (cn == 't' || cn == 'f') {
                        types[key] = 1; // boolean
                        value = cn;
                        values[key] = value;
                        key.clear();
                        value.clear();
                    } else {
                        type = 2;
                        types[key] = 2; // number
                        inValue = true;
                        value += cn;
                    }
                } else if (cn == '#') {
                    type = 3;
                    types[key] = 3; // vector
                    inValue = true;
                } else if (cn == ';') {
                    type = 0xFF;
                    i++;
                    if (i >= len) {
                        break;
                    }
                    cn = text[i];
                    if (cn == '"') {
                        inKey = true;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            }
        }
        if (inValue) {
            values[key] = value;
            key.clear();
            value.clear();
        }
        return true;

    }

    void CRequestParser::split(const std::string& text, const char& token, std::vector < std::string > & result ) const {
        result.resize(0);
        if (text.size() == 0) {
            return;
        }
        for( std::string::size_type p = 0, q = 0; p != text.npos; p=q ) {
            result.push_back( text.substr( p + ( p != 0 ), ( q = text.find(token, p + 1) ) - p - ( p != 0 ) ) );
        }
    }

    std::string CRequestParser::getStringValue(const std::string& key) {
        std::string result;
        std::map<std::string, std::string>::const_iterator iter = values.find(key);
        if ( iter != values.end() ) {
            result.assign(iter->second);
        } else {
            valid = false;
            atl::SysLog()->LogError(LOG_CONTEXT, "no string field `%s` in request", key.c_str() ) ;
        }
        return result;
    }

    atl::UInt32 CRequestParser::getUInt32Value(const std::string& key, const atl::basebool& hard) {
        atl::UInt32 result = 0;
        std::map<std::string, std::string>::const_iterator iter = values.find(key);
        if ( iter != values.end() ) {
            std::map<std::string, atl::UInt8>::const_iterator type = types.find(key);
            if ( type != types.end() && type->second == 2 ) { // is number
                result = atl::stou32(iter->second);
            } else {
                if (hard) {
                    valid = false;
                    atl::SysLog()->LogError(LOG_CONTEXT, "field `%s` exists in request but is not an integer", key.c_str() ) ;
                }
            }
        } else {
            if (hard) {
                valid = false;
                atl::SysLog()->LogError(LOG_CONTEXT, "no number field `%s` in request", key.c_str() ) ;
            }
        }
        return result;
    }

    std::string CRequestParser::getArrayValue(const std::string& key) {
        std::string result;
        std::map<std::string, std::string>::const_iterator iter = values.find(key);
        if ( iter != values.end() ) {
            std::map<std::string, atl::UInt8>::const_iterator type = types.find(key);
            if ( type != types.end() && type->second == 3 ) { // is vector
                result = iter->second;
            } else {
                valid = false;
                atl::SysLog()->LogError(LOG_CONTEXT, "field `%s` exists in request but is not an array", key.c_str() ) ;
            }
        } else {
            valid = false;
            atl::SysLog()->LogError(LOG_CONTEXT, "no array field `%s` in request", key.c_str() ) ;
        }
        return result;
    }

    atl::basebool CRequestParser::getBoolValue(const std::string& key) {
        atl::basebool result = false;
        std::map<std::string, std::string>::const_iterator iter = values.find(key);
        if ( iter != values.end() ) {
            result = (iter->second == "t");
        } else {
            valid = false;
            atl::SysLog()->LogError(LOG_CONTEXT, "no boolean field `%s` in request", key.c_str() ) ;
        }
        return result;
    }

    std::vector<atl::UInt8> CRequestParser::parseBytes(const std::string& text) {
        std::vector< atl::UInt8 > result;
        result.resize(0);
        if (valid) {
            std::vector < std::string > items;
            split(text, ',', items);
            atl::basesize asize = items.size();
            atl::UInt8 bt;
            for (atl::basesize idx = 0 ; idx < asize ; idx ++ ) {
                bt = atl::stou8(items.at(idx));
                result.push_back(bt) ;
            }
        }
        return result;
    }

    atl::basebool CRequestParser::isValid() const {
        return valid;
    }

    std::vector<atl::UInt8> CRequestParser::GetData() {
        std::string array = getArrayValue("data");
        return parseBytes(array);
    }

    std::vector<atl::UInt8> CRequestParser::GetMask() {
        std::string array = getArrayValue("mask");
        return parseBytes(array);
    }

    atl::UInt32 CRequestParser::GetOffset() {
        return getUInt32Value("offset");
    }

    atl::UInt32 CRequestParser::GetSize() {
        return getUInt32Value("size");
    }

    std::string CRequestParser::GetType() {
        return getStringValue("type");
    }

    atl::UInt8 CRequestParser::GetSection() {
        return getUInt32Value("sec");
    }

    atl::UInt8 CRequestParser::GetCommand() {
        return getUInt32Value("cmd");
    }

    atl::UInt32 CRequestParser::GetPage(const atl::basebool& hard) {
        return getUInt32Value("page", hard);
    }

    atl::TSmartPtr< CAccessManagerConfig > CRequestParser::GetAccessManagerConfig() {
        atl::TSmartPtr< CAccessManagerConfig > cfg = new CAccessManagerConfig() ;
        cfg->initialized = getBoolValue("init");
        if(cfg->initialized) {
            cfg->hasDebugOffset = (values.find("doff") != values.end());
            if (cfg->hasDebugOffset) {
                cfg->debugOffset = getUInt32Value("doff", false);
            }
            cfg->has1K = (values.find("boff") != values.end() && values.find("size") != values.end());
            if (cfg->has1K) {
                cfg->hwBufOffset = getUInt32Value("boff", false);
                cfg->hwBufSize = getUInt32Value("size", false);
            }
        } else {
            valid = false;
        }
        return cfg;
    }

    std::vector< atl::TSmartPtr < CRegPollCommand > > CRequestParser::GetRegPollCommands() {
        std::vector< atl::TSmartPtr < CRegPollCommand > > result;
        std::string text = getArrayValue("data");
        if (valid) {
            std::vector < std::string > items;
            split(text, ',', items);
            atl::basesize asize = items.size();
            if (asize % 4 == 0) {
                std::string tmp;
                for (atl::basesize s = 0; s < asize; s+=4) {
                    atl::TSmartPtr< CRegPollCommand > cmd = new CRegPollCommand() ;
                    tmp = items[s];
                    cmd->name = tmp.substr(1, tmp.size() - 2);
                    tmp = items[s + 1];
                    cmd->type = tmp.substr(1, 1);
                    cmd->offset = atl::stou32(items[s + 2]);
                    cmd->size = atl::stou32(items[s + 3]);
                    result.push_back(cmd);
                }
            } else {
                valid = false;
            }
        }
        return result;
    }

    std::vector< atl::TSmartPtr < CApiPollCommand > > CRequestParser::GetApiPollCommands() {
        std::vector< atl::TSmartPtr < CApiPollCommand > > result;
        std::string text = getArrayValue("data");
        if (valid) {
            std::vector < std::string > items;
            split(text, ',', items);
            atl::basesize asize = items.size();
            if (asize % 2 == 0) {
                for (atl::basesize s = 0; s < asize; s+=2) {
                    atl::TSmartPtr< CApiPollCommand > cmd = new CApiPollCommand() ;
                    cmd->section = atl::stou8(items[s]);
                    cmd->command = atl::stou8(items[s + 1]);
                    cmd->name = items[s] + ":" + items[s + 1];
                    result.push_back(cmd);
                }
            } else {
                valid = false;
            }
        }
        return result;
    }

}

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

#ifndef REQUESTPARSER_H
#define REQUESTPARSER_H

#include <ATL/ATLTypes.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>

#include "AccessManagerConfig.h"
#include "ClientCommand.h"

#include <vector>
#include <map>

namespace act {

    class CRequestParser : public virtual atl::CATLObject {

    private:
        std::map < std::string, std::string > values;
        std::map < std::string, atl::UInt8 > types;
        atl::basebool valid;

        atl::basebool parse(const std::string &text);
        void split(const std::string& text, const char& token, std::vector < std::string > & result) const ;

        std::string getStringValue(const std::string& key) ;
        atl::UInt32 getUInt32Value(const std::string& key, const atl::basebool& hard = true) ;
        std::string getArrayValue(const std::string& key) ;
        atl::basebool getBoolValue(const std::string& key) ;
        std::vector<atl::UInt8> parseBytes(const std::string& text) ;

    public:
        CRequestParser(const std::string& text);

        const std::string GetObjectStaticName() ;

        atl::basebool isValid() const;

        atl::TSmartPtr< CAccessManagerConfig > GetAccessManagerConfig() ;

        std::vector<atl::UInt8> GetData();    // data
        std::vector<atl::UInt8> GetMask();    // mask
        std::string GetType();             // type
        atl::UInt32 GetOffset();              // offset
        atl::UInt32 GetSize();                // size

        atl::UInt8 GetSection();              // sec
        atl::UInt8 GetCommand();              // cmd
        atl::UInt32 GetPage(const atl::basebool &hard = false); // page
        atl::UInt32 GetLength();              // len

        std::vector< atl::TSmartPtr < CRegPollCommand > > GetRegPollCommands();
        std::vector< atl::TSmartPtr < CApiPollCommand > > GetApiPollCommands();


    protected:
        virtual ~CRequestParser() ;

    };


}

#endif // REQUESTPARSER_H

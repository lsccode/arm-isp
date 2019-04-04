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

#ifndef RESPONSEWRITER_H
#define RESPONSEWRITER_H

#include <ATL/ATLTypes.h>
#include <ATL/ATLObject.h>
#include <ATL/ATLTemplates.h>

#include <vector>
#include <set>

namespace act {

    class CResponseWriter : public virtual atl::CATLObject {
    private:
        std::string text;
        atl::basebool started;
        atl::basebool firstField;
        std::set<std::string> keys;

        void addComma();

        static const std::string comma;
        static const std::string quote;
        static const std::string lbrace;
        static const std::string rbrace;
        static const std::string colon;
        static const std::string eqstr;
        static const std::string eqnum;
        static const std::string eqarr;
        static const std::string endarr;

    public:
        CResponseWriter();
        const std::string GetObjectStaticName();

        const std::string serialize() const ;
        atl::CATLError StartWriting();
        atl::CATLError FinishWriting();
        atl::CATLError AddField(const std::string& name, const std::string& value);
        atl::CATLError AddField(const std::string& name, const atl::UInt32& value);
        atl::CATLError AddDataArray(const std::string& name, std::vector<atl::UInt8> &values);

    protected:
        virtual ~CResponseWriter();

    };

}

#endif // RESPONSEWRITER_H

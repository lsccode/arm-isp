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

#ifndef __ACT_SERVER_APP__
#define __ACT_SERVER_APP__

#include <string>
#include <vector>

class CACTServerApp : public atl::CATLApplication {
private:
    virtual atl::CATLError CheckForImmediateAction();
public:
    CACTServerApp() {}
    ~CACTServerApp() {}

    virtual void ShowApplicationTitle();
    virtual void ShowUsage();
    virtual void ShowVersion();
    virtual atl::CATLError Initialize();
    virtual atl::CATLError InitCommands();
    virtual atl::CATLError CreateCommand(const std::string &name);

};

#endif // __ACT_SERVER_APP__

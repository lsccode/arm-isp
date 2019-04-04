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

#ifndef ACTDIRECTCOMMAND_H
#define ACTDIRECTCOMMAND_H

#include <ATL/ATLApplication.h>
#include <ATL/ATLTemplates.h>
#include <ATL/ATLComponent.h>

#include <AccessManager/AccessManager.h>

using namespace atl;

class CACTDirectCommand : public virtual CATLApplicationCommand {
private:
    TSmartPtr<act::CAccessManager> am;

    basebool isAcessManagerConfigured() const;

protected:
    virtual ~CACTDirectCommand() {}

public:
    CACTDirectCommand();
    virtual CATLError CheckForImmediateAction();
    virtual CATLError Execute();
    virtual CATLError Stop();
    static void ShowUsage();

};

#endif // ACTDIRECTCOMMAND_H


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

#ifndef __ATL_BASE_APPLICATION__
#define __ATL_BASE_APPLICATION__

#include "ATLTypes.h"
#include "ATLError.h"
#include "ATLObject.h"
#include "ATLLogger.h"
#include "ATLTemplates.h"
#include "ATLConfig.h"

#include <string>
#include <iostream>

namespace atl {

    typedef enum __EATLApplicationStates {
        EATLApplicationNotInitialized = 0,
        EATLApplicationClosed,
        EATLApplicationRunning,
        EATLApplicationStopping,
        EATLApplicationStopped
    } EATLApplicationStates;


    class CATLApplicationCommand : public virtual CATLObject {

    public:
        CATLApplicationCommand() {}
        inline virtual const std::string GetObjectStaticName() { return "CATLApplicationCommand"; }

    public:
        virtual CATLError Execute() = 0;
        virtual CATLError Stop() = 0;
        static void ShowUsage() {}

    protected:
        virtual ~CATLApplicationCommand() {}

    };

    class CATLApplication {

    private:
        volatile EATLApplicationStates state;

    protected:
        TSmartPtr<CATLApplicationCommand> command;
        CATLApplication() : state(EATLApplicationNotInitialized) {}
        virtual CATLError CheckForImmediateAction();

    public:
        virtual ~CATLApplication() {}

    private:
        virtual CATLError InitLog();
        virtual CATLError ApplyPreset();

    protected:
        virtual CATLError ParseParameters(const std::vector<std::string>& args);
        virtual CATLError Initialize() = 0;
        virtual CATLError Run();

        virtual void ShowApplicationTitle() = 0;
        virtual void ShowUsage() = 0;
        virtual void ShowVersion() = 0;
        virtual CATLError InitCommands() = 0;
        virtual CATLError CreateCommand(const std::string& name) = 0;

    public:
        virtual CATLError Main(const std::vector<std::string>& args);
        virtual CATLError Stop();

    };

} // atl namespace

#endif // __ATL_BASE_APPLICATION__

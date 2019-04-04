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

#define LOG_CONTEXT "ACTServer"

#include <ATL/ATLApplication.h>

#include "ACTServerApp.h"

#if defined( _WIN32 ) && defined( _DEBUG )
#include <crtdbg.h>
#endif

#include <string.h>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <tchar.h>
#include <windows.h>
#endif

using namespace atl;

static CACTServerApp* application = NULL ;

#ifdef _WIN32
BOOL WINAPI ConsoleCtrlHandlerRoutine( DWORD reason ) {
    if( application != NULL ) {
        switch( reason ) {
            case CTRL_C_EVENT :
            case CTRL_BREAK_EVENT :
            case CTRL_CLOSE_EVENT :
            case CTRL_LOGOFF_EVENT :
            case CTRL_SHUTDOWN_EVENT :
                application->Stop() ;
                return TRUE;
        }
    }
    return FALSE;
}
#else
#define _tmain main
#define _TCHAR char
#endif // _WIN32

int _tmain(int argc, _TCHAR* argv[]) {

#if defined( _WIN32 ) && defined( _DEBUG )
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|/*_CRTDBG_CHECK_ALWAYS_DF|*/_CRTDBG_LEAK_CHECK_DF|/*_CRTDBG_DELAY_FREE_MEM_DF|*/_CRTDBG_CHECK_CRT_DF);
#endif // defined( _WIN32 ) && defined( _DEBUG )

    application = new CACTServerApp();

#ifdef _WIN32
    SetConsoleCtrlHandler(&ConsoleCtrlHandlerRoutine, TRUE);
#endif // _WIN32

    std::vector<std::string> args;
    for (int i = 0 ; i < argc ; i ++) {
        args.push_back(std::string(argv[i])) ;
    }

    CATLError result = application->Main(args);

#ifdef _WIN32
    SetConsoleCtrlHandler(&ConsoleCtrlHandlerRoutine, FALSE);
#endif // _WIN32

    delete application;
    CATLError::ClearLastError();

    return result.GetErrorCode();
}


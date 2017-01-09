#include <Windows.h>
#include <GWCA/GWCA.h>

#include "CommandHandler/CommandHandler.h"
#include "DropsHandler/DropsHandler.h"
#include "TitleUpdater/TitleUpdater.h"

HANDLE commandHandlerThread = nullptr;
HANDLE dropsHandlerThread = nullptr;
HANDLE titleUpdaterThread = nullptr;

using namespace gbr::InProcess;

BOOL WINAPI DllMain(_In_ HMODULE hModule, _In_ DWORD reason, _In_opt_ LPVOID reserved) {
    DisableThreadLibraryCalls(hModule);

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        if (!GW::Api::Initialize())
            return FALSE;

        commandHandlerThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)CommandHandler::ThreadEntry, hModule, 0, 0);
        dropsHandlerThread = CreateThread(0, 0, DropsHandler::ThreadEntry, hModule, 0, 0);
        //titleUpdaterThread = CreateThread(0, 0, TitleUpdater::ThreadEntry, hModule, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        if (commandHandlerThread)
            TerminateThread(commandHandlerThread, EXIT_SUCCESS);

        if (dropsHandlerThread)
            TerminateThread(dropsHandlerThread, EXIT_SUCCESS);

        if (titleUpdaterThread)
            TerminateThread(titleUpdaterThread, EXIT_SUCCESS);

        GW::Api::Destruct();
        break;
    }

    return TRUE;
}
#include <Windows.h>

#include "Root.h"

using namespace gbr::InProcess;

void Start(HMODULE hModule) {
    if (*(DWORD*)0x00DE0000 != NULL) {
        MessageBoxA(0, "Please restart guild wars and try again.", "gbr Error", 0);
        FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
    }

    while (*(void**)0xA2B294 == nullptr) {
        Sleep(100);
    }

    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Root::ThreadStart, hModule, 0, 0);

    return;
}

BOOL WINAPI DllMain(_In_ HMODULE hModule, _In_ DWORD reason, _In_opt_ LPVOID reserved) {
    DisableThreadLibraryCalls(hModule);

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Start, hModule, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        Root::Quit();
        break;
    }

    return TRUE;
}
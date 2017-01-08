#include <ctime>
#include <Windows.h>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/PlayerMgr.h>

#include "TitleUpdater.h"

namespace gbr::InProcess {
    DWORD WINAPI TitleUpdater::ThreadEntry(LPVOID) {
        wchar_t buffer[0x100];
        auto gwHandle = GW::MemoryMgr::GetGWWindowHandle();
        auto start_time = time(nullptr);

        while (true) {
            auto seconds = time(nullptr) - start_time;
            auto minutes = (seconds / 60) % 60;
            auto hours = (seconds / 3600);
            seconds = seconds % 60;

            auto charName = GW::Playermgr().GetPlayerName(0); // (wchar_t*)0x00A2AE80;

            swprintf_s(buffer, L"GW - %s - [%d:%02d:%02d]", charName, hours, minutes, seconds);
            SetWindowTextW(gwHandle, buffer);

            Sleep(1000);
        }

        return TRUE;
    }
}
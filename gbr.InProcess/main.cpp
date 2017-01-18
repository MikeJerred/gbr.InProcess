#include <Windows.h>
#include <GWCA/GWCA.h>

#include "Handlers/BonderHandler.h"
#include "Handlers/CommandHandler.h"
#include "Handlers/DropsHandler.h"
#include "Handlers/SpikerHandler.h"
#include "Utilities/PlayerUtility.h"

using namespace gbr::InProcess;

BonderHandler* bonderHandler = nullptr;
CommandHandler* commandHandler = nullptr;
DropsHandler* dropsHandler = nullptr;
SpikerHandler* spikerHandler = nullptr;

DWORD WINAPI Start(HMODULE hModule) {
    if (!GW::Api::Initialize())
        return FALSE;

    auto playerName = GW::Agents().GetPlayerNameByLoginNumber(GW::Agents().GetPlayer()->LoginNumber);
    auto playerType = Utilities::PlayerUtility::GetPlayerType();

    commandHandler = new CommandHandler(hModule, playerName);
    dropsHandler = new DropsHandler();

    switch (playerType) {
    case Utilities::PlayerType::VoR:
    case Utilities::PlayerType::ESurge1:
    case Utilities::PlayerType::ESurge2:
    case Utilities::PlayerType::ESurge3:
    case Utilities::PlayerType::ESurge4:
        spikerHandler = new SpikerHandler(playerType);
        break;
    case Utilities::PlayerType::Monk:
    case Utilities::PlayerType::Emo:
        bonderHandler = new BonderHandler(playerType);
        break;
    }

    return TRUE;
}

BOOL WINAPI DllMain(_In_ HMODULE hModule, _In_ DWORD reason, _In_opt_ LPVOID reserved) {
    DisableThreadLibraryCalls(hModule);

    switch (reason) {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Start, hModule, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        if (bonderHandler)
            delete bonderHandler;

        if (commandHandler)
            delete commandHandler;

        if (dropsHandler)
            delete dropsHandler;

        if (spikerHandler)
            delete spikerHandler;

        GW::Api::Destruct();
        break;
    }

    return TRUE;
}
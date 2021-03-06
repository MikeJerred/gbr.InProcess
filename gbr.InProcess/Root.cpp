#include <GWCA/GWCA.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>

#include "Root.h"

namespace gbr::InProcess {
    Root* Root::instance = nullptr;
    bool Root::mustQuit = false;

    Root::Root(HMODULE hModule) {
        auto playerName = GW::Agents::GetPlayerNameByLoginNumber(GW::Agents::GetPlayer()->LoginNumber);
        auto playerType = Utilities::PlayerUtility::GetPlayerType();

        Utilities::LogUtility::Init(playerName);

        commandHandler = new CommandHandler(hModule, playerName);
        dropsHandler = new DropsHandler();
        tradeHandler = new TradeHandler();

        switch (playerType) {
        case Utilities::PlayerType::VoR:
        case Utilities::PlayerType::ESurge1:
        case Utilities::PlayerType::ESurge2:
        case Utilities::PlayerType::ESurge3:
        case Utilities::PlayerType::ESurge4:
            spikerHandler = new SpikerHandler(playerType);
            break;
        case Utilities::PlayerType::Monk:
            monkHandler = new MonkHandler(playerType);
            break;
        case Utilities::PlayerType::Emo:
            bonderHandler = new BonderHandler(playerType);
            break;
        }

        mustQuit = false;
    }

    Root::~Root() {
        if (bonderHandler)
            delete bonderHandler;

        if (commandHandler)
            delete commandHandler;

        if (dropsHandler)
            delete dropsHandler;

        if (monkHandler)
            delete monkHandler;

        if (spikerHandler)
            delete spikerHandler;

        if (tradeHandler)
            delete tradeHandler;

        Utilities::LogUtility::Close();
    }

    void Root::ThreadStart(HMODULE hModule) {
        if (instance) {
            MessageBoxA(0, "Already loaded.", "gbr Error", 0);
            FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
        }

        if (!GW::Initialize()) {
            MessageBoxA(0, "Failed to load GWCA.", "gbr Error", 0);
            FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
        }

        instance = new Root(hModule);

        GW::GameThread::ToggleRenderHook();

        while (!mustQuit) {
            Sleep(50);

            if (GetAsyncKeyState(VK_END) & 1) {
                mustQuit = true;
            }

            if (GetAsyncKeyState(VK_HOME) & 1) {
                GW::GameThread::ToggleRenderHook();
				Sleep(500);
            }
        }

        if (instance)
            delete instance;

        std::this_thread::sleep_for(std::chrono::seconds(1));

        GW::Terminate();
        FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
    }

    void Root::Quit() {
        mustQuit = true;
    }
}
#include "Root.h"

namespace gbr::InProcess {
    Root* Root::instance = nullptr;
    bool Root::mustQuit = false;

    Root::Root(HMODULE hModule) {
        auto playerName = GW::Agents().GetPlayerNameByLoginNumber(GW::Agents().GetPlayer()->LoginNumber);
        auto playerType = Utilities::PlayerUtility::GetPlayerType();

		Utilities::LogUtility::Init(playerName);

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

        mustQuit = false;
    }

    Root::~Root() {
        if (bonderHandler)
            delete bonderHandler;

        if (commandHandler)
            delete commandHandler;

        if (dropsHandler)
            delete dropsHandler;

        if (spikerHandler)
            delete spikerHandler;

		Utilities::LogUtility::Close();
    }

    void Root::ThreadStart(HMODULE hModule) {
        if (instance) {
            MessageBoxA(0, "Already loaded.", "gbr Error", 0);
            FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
        }

        if (!GW::Api::Initialize()) {
            MessageBoxA(0, "Failed to load GWCA.", "gbr Error", 0);
            FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
        }

        instance = new Root(hModule);

        GW::Gamethread().ToggleRenderHook();

        while (!mustQuit) {
            Sleep(250);

            if (GetAsyncKeyState(VK_END) & 1) {
                //mustQuit = true;
            }

            if (GetAsyncKeyState(VK_HOME) & 1) {
                GW::Gamethread().ToggleRenderHook();
            }
        }

        if (instance)
            delete instance;

        std::this_thread::sleep_for(std::chrono::seconds(1));

        GW::Api::Destruct();
        FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
    }

    void Root::Quit() {
        mustQuit = true;
    }
}
#pragma once

#include <Windows.h>
#include <GWCA/GWCA.h>

#include "Handlers/BonderHandler.h"
#include "Handlers/CommandHandler.h"
#include "Handlers/DropsHandler.h"
#include "Handlers/MonkHandler.h"
#include "Handlers/SpikerHandler.h"
#include "Handlers/TradeHandler.h"
#include "Utilities/LogUtility.h"
#include "Utilities/PlayerUtility.h"

namespace gbr::InProcess {

    class Root {
    private:
        static Root* instance;
        static bool mustQuit;

        BonderHandler* bonderHandler;
        CommandHandler* commandHandler;
        DropsHandler* dropsHandler;
        MonkHandler* monkHandler;
        SpikerHandler* spikerHandler;
        TradeHandler* tradeHandler;

        Root(HMODULE hModule);
        ~Root();
    public:
        static void ThreadStart(HMODULE hModule);
        static void Quit();
    };
}
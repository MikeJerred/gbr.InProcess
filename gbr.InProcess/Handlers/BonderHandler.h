#pragma once

#include <Windows.h>
#include <GWCA/GWCA.h>
#include <GWCA/GameEntities/Skill.h>
#include "../Utilities/PlayerUtility.h"

namespace gbr::InProcess {
    class BonderHandler {
    private:
        Utilities::PlayerType playerType;
        GUID hookGuid;

        bool ShouldSleep(long long& sleepUntil);
        void TickMonk();
        void TickEmo();

        bool HasABond(DWORD loginNumber, std::vector<GW::Buff> bonds);
    public:
        BonderHandler(Utilities::PlayerType playerType);
        ~BonderHandler();
    };
}
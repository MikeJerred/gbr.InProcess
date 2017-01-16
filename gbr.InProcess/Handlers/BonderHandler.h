#pragma once

#include <Windows.h>
#include "../Utilities/PlayerUtility.h"

namespace gbr::InProcess {
    class BonderHandler {
    private:
        Utilities::PlayerType playerType;
        GUID hookGuid;

        bool ShouldSleep(long long& sleepUntil);
        void TickMonk();
        void TickEmo();
    public:
        BonderHandler(Utilities::PlayerType playerType);
        ~BonderHandler();
    };
}
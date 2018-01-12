#pragma once

#include <Windows.h>
#include <vector>
#include <GWCA/GameEntities/Agent.h>

namespace gbr::InProcess {
    class TradeHandler {
    private:
        enum class State {
            Waiting,
            StartingTrade,
            InTrade,
            FinishTrade
        };

        static std::vector<int> itemsToTrade;
        DWORD hookId;
        State state;
        std::vector<DWORD> tradingItemIds;

        void Tick();
        void WaitingTick();
        void StartingTradeTick(GW::Agent* tank);
        void InTradeTick();
        void FinishTradeTick();
    public:
        TradeHandler();
        ~TradeHandler();
    };
}
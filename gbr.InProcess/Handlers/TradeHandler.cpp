#include <GWCA/GWCA.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/TradeMgr.h>

#include "TradeHandler.h"

namespace gbr::InProcess {
    std::vector<int> TradeHandler::itemsToTrade = {
        GW::Constants::ItemID::TormentGem,
        GW::Constants::ItemID::MargoniteGem,
        GW::Constants::ItemID::TitanGem,
        GW::Constants::ItemID::StygianGem,
        GW::Constants::ItemID::LunarToken
    };

    TradeHandler::TradeHandler() {
        state = State::Waiting;
        tradingItemIds = std::vector<DWORD>();

        hookId = GW::Gamethread().AddPermanentCall([this]() {
            Tick();
        });
    }

    TradeHandler::~TradeHandler() {
        GW::Gamethread().RemovePermanentCall(hookId);
    }

    void TradeHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0) {
            return;
        }

        sleepUntil = currentTime + 1000;

        auto player = GW::Agents().GetPlayer();
        if (!player || !GW::Map().IsMapLoaded() || GW::Map().GetInstanceType() != GW::Constants::InstanceType::Outpost) {
            state = State::Waiting;
            return;
        }

        auto agents = GW::Agents().GetAgentArray();
        auto partyInfo = GW::Partymgr().GetPartyInfo();
        if (!agents.valid() || !partyInfo || !partyInfo->players.valid() || !GW::GameContext::instance()->trade)
            return;

        auto partyMembers = partyInfo->players;

        GW::Agent* tank = nullptr;
        for (auto p : partyMembers) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);

            if (agent
                && (agent->Primary == (BYTE)GW::Constants::Profession::Ranger
                    || agent->Primary == (BYTE)GW::Constants::Profession::Assassin)) {

                    tank = agent;
            }
        }

        if (!tank)
            return;

        switch (state) {
        case State::Waiting:
            WaitingTick();
            break;
        case State::StartingTrade:
            StartingTradeTick(tank);
            break;
        case State::InTrade:
            InTradeTick();
            break;
        case State::FinishTrade:
            FinishTradeTick();
            break;
        }
    }

    void TradeHandler::WaitingTick() {
        auto tradeContext = GW::GameContext::instance()->trade;

        if (tradeContext->state == tradeContext->TRADE_INITIATED) {
            GW::Trademgr().CancelTrade();
            state = State::StartingTrade;
        }
    }

    void TradeHandler::StartingTradeTick(GW::Agent* tank) {
        GW::Trademgr().OpenTradeWindow(tank->Id);
        state = State::InTrade;
    }

    void TradeHandler::InTradeTick() {
        auto tradeContext = GW::GameContext::instance()->trade;
        if (tradeContext->state != tradeContext->TRADE_INITIATED || !GW::Items().GetBagArray())
            return;

        GW::Item* item = nullptr;

        for (auto modelId : itemsToTrade) {
            item = GW::Items().GetItemByModelId(modelId);

            if (item) {
                for (auto tradedItemId : tradingItemIds) {
                    if (tradedItemId == item->ItemId) {
                        item = nullptr;
                        break;
                    }
                }

                if (item) {
                    break;
                }
            }
        }

        if (!item) {
            GW::Trademgr().SubmitOffer(0);
            state = State::FinishTrade;
        }
        else {
            tradingItemIds.push_back(item->ItemId);
            GW::Trademgr().OfferItem(item->ItemId, item->Quantity);
        }
    }

    void TradeHandler::FinishTradeTick() {
        auto tradeContext = GW::GameContext::instance()->trade;

        if (tradeContext->state == tradeContext->NO_TRADE) {
            state = State::Waiting;
            return;
        }

        if (tradeContext->partner.gold > 0) {
            GW::Trademgr().AcceptTrade();
            tradingItemIds.clear();
            state = State::Waiting;
        }
    }
}
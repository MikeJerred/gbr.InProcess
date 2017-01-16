#include <chrono>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/ItemMgr.h>

#include "DropsHandler.h"

namespace gbr::InProcess {
    const std::initializer_list<int> DropsHandler::wantedModelIds = {
        GW::Constants::ItemID::StygianGem,
        GW::Constants::ItemID::TormentGem,
        GW::Constants::ItemID::TitanGem,
        GW::Constants::ItemID::MargoniteGem
    };

    DropsHandler::DropsHandler() {
        hookGuid = GW::Gamethread().AddPermanentCall([]() {
            Tick();
        });
    }

    DropsHandler::~DropsHandler() {
        GW::Gamethread().RemovePermanentCall(hookGuid);
    }

    void DropsHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();

        if ((sleepUntil - currentTime) > 0) {
            return;
        }

        sleepUntil = currentTime + 250;

        auto player = GW::Agents().GetPlayer();

        if (player && !player->GetIsMoving()) {
            if (PickupNearbyGem()) {
                sleepUntil = currentTime + 2000;
            }
        }
    }

    bool DropsHandler::PickupNearbyGem() {
        auto player = GW::Agents().GetPlayer();

        if (player) {
            auto pos = player->pos;

            auto agents = GW::Agents().GetAgentArray();
            auto items = GW::Items().GetItemArray();
            const float adjacentSq = GW::Constants::Range::Adjacent * GW::Constants::Range::Adjacent;
            const float nearbySq = GW::Constants::Range::Nearby * GW::Constants::Range::Nearby;

            if (agents.valid() && items.valid()) {
                GW::Agent* chest = nullptr;

                for (auto agent : agents) {
                    if (!agent || agent->pos.SquaredDistanceTo(pos) > nearbySq)
                        continue;

                    if (agent->GetIsItemType()) {
                        auto item = items[agent->itemid];

                        if (agent->Owner == player->Id) {
                            for (auto modelId : wantedModelIds) {
                                if (item->ModelId == modelId) {
                                    if (agent->pos.SquaredDistanceTo(pos) < adjacentSq) {
                                        GW::Items().PickUpItem(item);
                                        return true;
                                    }
                                    else {
                                        GW::Agents().Move(agent->pos);
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                    else if (agent->GetIsSignpostType()) {
                        // prioritize picking up gems over opening chest
                        chest = agent;
                    }
                }

                if (chest) {
                    if (chest->pos.SquaredDistanceTo(pos) < adjacentSq) {
                        GW::Agents().GoSignpost(chest);
                        return true;
                    }
                    else {
                        GW::Agents().Move(chest->pos);
                        return false;
                    }
                }
            }
        }

        return false;
    }
}
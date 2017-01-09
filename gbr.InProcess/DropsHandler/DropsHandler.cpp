#include <chrono>
#include <mutex>
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

    DWORD WINAPI DropsHandler::ThreadEntry(LPVOID) {
        auto instance = DropsHandler();

        instance.Listen();

        return TRUE;
    }

    void DropsHandler::Listen() {
        bool waitingForCall = false;

        while (true) {
            if (!waitingForCall) {
                waitingForCall = true;

                GW::Gamethread().Enqueue([&]() {
                    auto player = GW::Agents().GetPlayer();

                    if (player && !player->GetIsMoving()) {
                        PickupNearbyGem();
                    }

                    waitingForCall = false;
                });
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    void DropsHandler::PickupNearbyGem() {
        auto player = GW::Agents().GetPlayer();

        if (player) {
            auto pos = player->pos;

            auto agents = GW::Agents().GetAgentArray();
            auto items = GW::Items().GetItemArray();
            const float adjacentSq = GW::Constants::Range::Adjacent * GW::Constants::Range::Adjacent;
            const float nearbySq = GW::Constants::Range::Nearby * GW::Constants::Range::Nearby;

            if (agents.valid() && items.valid()) {
                for (auto agent : agents) {
                    if (agent && agent->GetIsItemType() && agent->pos.SquaredDistanceTo(pos) < nearbySq) {
                        auto item = items[agent->itemid];

                        if (item && agent->Owner == 0 || agent->Owner == player->Id) {
                            for (auto modelId : wantedModelIds) {
                                if (item->ModelId == modelId) {
                                    if (agent->pos.SquaredDistanceTo(pos) < adjacentSq) {
                                        GW::Items().PickUpItem(item);
                                    }
                                    else {
                                        GW::Agents().Move(agent->pos);
                                    }

                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
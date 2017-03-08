#include <chrono>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <gbr.Shared/Commands/MoveTo.h>

#include "DropsHandler.h"

namespace gbr::InProcess {
    const std::initializer_list<int> DropsHandler::wantedModelIds = {
        GW::Constants::ItemID::StygianGem,
        GW::Constants::ItemID::TormentGem,
        GW::Constants::ItemID::TitanGem,
        GW::Constants::ItemID::MargoniteGem,
        GW::Constants::ItemID::LunarToken
    };

    DropsHandler::DropsHandler() {
        hookId = GW::Gamethread().AddPermanentCall([]() {
            Tick();
        });
    }

    DropsHandler::~DropsHandler() {
        GW::Gamethread().RemovePermanentCall(hookId);
    }

    void DropsHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0) {
            return;
        }

        sleepUntil = currentTime + 250;

        auto player = GW::Agents().GetPlayer();
        if (!player)
            return;

        auto pos = gbr::Shared::Commands::MoveTo::GetPos();
        if (pos.HasValue()) {
            if (player->pos.SquaredDistanceTo(pos.Value()) < GW::Constants::SqrRange::Adjacent) {
                gbr::Shared::Commands::MoveTo::ClearPos();
            }
            else if (!GW::Skillbar::GetPlayerSkillbar().Casting) {
                GW::Agents().Move(pos.Value());
            }

            return;
        }


        if (player && !player->GetIsMoving()) {
            if (PickupNearbyGem()) {
                sleepUntil = currentTime + 1000;
            }
        }
    }

    bool DropsHandler::PickupNearbyGem() {
        auto player = GW::Agents().GetPlayer();

        if (player) {
            auto pos = player->pos;

            auto agents = GW::Agents().GetAgentArray();
            auto items = GW::Items().GetItemArray();

            if (agents.valid() && items.valid()) {
                GW::Agent* chest = nullptr;

                for (auto agent : agents) {
                    if (!agent || agent->pos.SquaredDistanceTo(pos) > GW::Constants::SqrRange::Nearby)
                        continue;

                    if (agent->GetIsItemType()) {
                        auto item = items[agent->itemid];

                        if (agent->Owner == player->Id) {
                            for (auto modelId : wantedModelIds) {
                                if (item->ModelId == modelId) {
                                    if (agent->pos.SquaredDistanceTo(pos) < GW::Constants::SqrRange::Adjacent) {
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
                    if (chest->pos.SquaredDistanceTo(pos) < GW::Constants::SqrRange::Adjacent) {
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
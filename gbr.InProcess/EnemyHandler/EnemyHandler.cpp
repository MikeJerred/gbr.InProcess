#include <algorithm>
#include <chrono>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <gbr.Shared/Commands/AggressiveMoveTo.h>

#include "EnemyHandler.h"

namespace gbr::InProcess {
    DWORD WINAPI EnemyHandler::ThreadEntry(LPVOID) {
        PlayerType type;
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;

        switch (skills[0].SkillId) {
        case (DWORD)GW::Constants::SkillID::Shadow_Form:
            type = PlayerType::Tank;
            break;
        case (DWORD)GW::Constants::SkillID::Visions_of_Regret:
            type = PlayerType::Vor;
            break;
        case (DWORD)GW::Constants::SkillID::Energy_Surge:
            type = PlayerType::ESurge;
            break;
        case (DWORD)GW::Constants::SkillID::Ether_Renewal:
        case (DWORD)GW::Constants::SkillID::Life_Barrier:
            type = PlayerType::Bonder;
            break;
        case (DWORD)GW::Constants::SkillID::Unyielding_Aura:
            type = PlayerType::Rez;
            break;
        default:
            type = PlayerType::Unknown;
            break;
        }

        auto instance = EnemyHandler(type);

        instance.Listen();

        return TRUE;
    }

    void EnemyHandler::Listen() {
        while (true) {
            GW::Gamethread().Enqueue([&]() {
                auto player = GW::Agents().GetPlayer();
                auto agents = GW::Agents().GetAgentArray();

                if (!player || player->GetIsDead() || !agents.valid())
                    return;

                const float adjacentSq = GW::Constants::Range::Adjacent * GW::Constants::Range::Adjacent;
                const float spellcastSq = GW::Constants::Range::Spellcast * GW::Constants::Range::Spellcast;

                auto enemyId = gbr::Shared::Commands::AggressiveMoveTo::GetTargetEnemyId();
                auto npcId = gbr::Shared::Commands::AggressiveMoveTo::GetTargetNpcId();
                if (enemyId) {
                    auto agent = agents[enemyId];
                    if (agent && !agent->GetIsDead()) {
                        if (agent->pos.SquaredDistanceTo(player->pos) < spellcastSq) {
                            // stop moving
                            GW::Agents().Move(player->pos);

                            SpikeTarget(agent);
                        }
                        else {
                            GW::Agents().Move(agent->pos);
                        }
                    }
                    else {
                        gbr::Shared::Commands::AggressiveMoveTo::SetTargetEnemyId(0);
                    }
                }
                else if (npcId) {
                    auto agent = agents[npcId];
                    if (agent && !agent->GetIsDead()) {
                        if (agent->pos.SquaredDistanceTo(player->pos) < adjacentSq) {
                            // stop moving
                            GW::Agents().Move(player->pos);

                            GW::Agents().GoNPC(agent);

                            // take quest etc.
                            SendDialog(agent);

                            gbr::Shared::Commands::AggressiveMoveTo::SetTargetNpcId(0);
                        }
                        else {
                            GW::Agents().GoNPC(agent);
                        }
                    }
                }
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    void EnemyHandler::SpikeTarget(GW::Agent* enemy) {
        if (playerType == PlayerType::Vor || playerType == PlayerType::ESurge) {
            auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;
            for (int i = 0; i < 8; ++i) {
                if (skills[i].GetRecharge() == 0) {
                    GW::Skillbarmgr().UseSkill(i, enemy->Id);
                }
            }
        }
    }

    void EnemyHandler::SendDialog(GW::Agent* npc) {
        GW::StoC().AddSingleGameServerEvent<GW::Packet::StoC::P114_DialogButton>([](GW::Packet::StoC::P114_DialogButton* packet) {
            GW::Agents().Dialog(packet->dialog_id);
            return false;
        });
    }
}
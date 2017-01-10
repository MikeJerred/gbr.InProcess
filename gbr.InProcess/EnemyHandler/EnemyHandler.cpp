#include <algorithm>
#include <chrono>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <gbr.Shared/Commands/AggressiveMoveTo.h>

#include "EnemyHandler.h"

namespace gbr::InProcess {
    DWORD WINAPI EnemyHandler::ThreadEntry(LPVOID) {
        SpikerType type;
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;

        switch (skills[0].SkillId) {
        case (DWORD)GW::Constants::SkillID::Visions_of_Regret:
            type = SpikerType::Vor;
            break;
        case (DWORD)GW::Constants::SkillID::Energy_Surge:
            type = SpikerType::ESurge;
            break;
        default:
            type = SpikerType::None;
            break;
        }

        if (type != SpikerType::None) {
            auto instance = EnemyHandler(type);

            instance.Listen();
        }

        return TRUE;
    }

    void EnemyHandler::Listen() {
        bool waitingForCall = false;

        while (true) {
            if (!waitingForCall) {
                waitingForCall = true;
                GW::Gamethread().Enqueue([&]() {
                    auto player = GW::Agents().GetPlayer();
                    auto agents = GW::Agents().GetAgentArray();

                    if (!player || player->GetIsDead() || !agents.valid())
                        return;

                    const float adjacentSq = GW::Constants::Range::Adjacent * GW::Constants::Range::Adjacent;
                    const float spellcastSq = GW::Constants::Range::Spellcast * GW::Constants::Range::Spellcast;

                    auto enemyId = gbr::Shared::Commands::AggressiveMoveTo::targetEnemyId;
                    auto npcId = gbr::Shared::Commands::AggressiveMoveTo::targetEnemyId;
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
                    }
                    else if (npcId) {
                        auto agent = agents[npcId];
                        if (agent && !agent->GetIsDead()) {
                            if (agent->pos.SquaredDistanceTo(player->pos) < adjacentSq) {
                                // stop moving
                                GW::Agents().Move(player->pos);

                                // take quest etc.
                            }
                            else {
                                GW::Agents().GoNPC(agent);
                            }
                        }
                    }

                    waitingForCall = false;
                });
            }

            std::this_thread::sleep_for(std::chrono::seconds(250));
        }
    }

    void EnemyHandler::SpikeTarget(GW::Agent* enemy) {
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;
        for (int i = 0; i < 8; i++) {
            auto skill = skills[i];

            if (skill.GetRecharge() == 0) {
                GW::Skillbarmgr().UseSkillByID(skill.SkillId, enemy->Id);
            }
        }
    }
}
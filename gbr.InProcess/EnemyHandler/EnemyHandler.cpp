#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
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

                auto id = gbr::Shared::Commands::AggressiveMoveTo::GetTargetAgentId();
                if (id) {
                    auto agent = agents[id];
                    if (agent) {
                        if (agent->GetIsItemType()) {
                            GW::Agents().Move(agent->pos);
                            return;
                        }

                        if (!agent->GetIsDead()) {
                            if (agent->Allegiance == 3) {
                                // kill the enemy
                                if (agent->pos.SquaredDistanceTo(player->pos) < spellcastSq) {
                                    GW::Agents().Move(player->pos); // stop moving
                                    SpikeTarget(agent);
                                }
                                else {
                                    GW::Agents().Move(agent->pos);
                                }

                                return;
                            }
                            else if (agent->IsNPC()) {
                                if (agent->pos.SquaredDistanceTo(player->pos) < adjacentSq) {
                                    GW::Agents().GoNPC(agent);
                                    SendDialog(agent);
                                    // after here we fall out of the if-else scopes and erase the target Id
                                }
                                else if (agent->pos.SquaredDistanceTo(player->pos) < spellcastSq) {
                                    JumpToTarget(agent);
                                    return;
                                }
                                else {
                                    GW::Agents().GoNPC(agent);
                                    return;
                                }
                            }
                            else if (agent->IsPlayer()) {
                                // jump to target
                                if (agent->pos.SquaredDistanceTo(player->pos) < adjacentSq) {
                                    // already at target, so fall through and set target Id to 0
                                }
                                if (agent->pos.SquaredDistanceTo(player->pos) < spellcastSq) {
                                    JumpToTarget(agent);
                                    return;
                                }
                                else {
                                    GW::Agents().Move(agent->pos);
                                    return;
                                }
                            }

                            // should never get here: any agent that is alive should be either an enemy, an NPC, or a player
                        }
                        else if (agent->IsPlayer()) {
                            // rez player
                            if (playerType == PlayerType::Rez) {
                                if (agent->pos.SquaredDistanceTo(player->pos) < spellcastSq) {
                                    if (!RessurectTarget(agent))
                                        return;
                                    // after here we fall out of the if-else scopes and erase the target Id
                                }
                                else {
                                    GW::Agents().Move(agent->pos);
                                    return;
                                }
                            }
                        }
                    }

                    gbr::Shared::Commands::AggressiveMoveTo::SetTargetAgentId(0);
                }
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    void EnemyHandler::JumpToTarget(GW::Agent* target) {
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;
        auto ee = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Ebon_Escape);

        if (ee.GetRecharge() == 0) {
            GW::Skillbarmgr().UseSkill(GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Ebon_Escape), target->Id);
        }
        else {
            GW::Agents().Move(target->pos);
        }
    }

    bool EnemyHandler::RessurectTarget(GW::Agent* target) {
        auto buff = GW::Effects().GetPlayerBuffBySkillId(GW::Constants::SkillID::Unyielding_Aura);
        if (buff.BuffId > 0) {
            GW::Effects().DropBuff(buff.BuffId);
            return true;
        }
        else {
            auto ua = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unyielding_Aura);
            if (ua.SkillId > 0 && ua.GetRecharge() == 0)
                GW::Skillbarmgr().UseSkill(GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unyielding_Aura));

            return false;
        }
    }

    void EnemyHandler::SendDialog(GW::Agent* npc) {
        GW::StoC().AddSingleGameServerEvent<GW::Packet::StoC::P114_DialogButton>([](GW::Packet::StoC::P114_DialogButton* packet) {
            GW::Agents().Dialog(packet->dialog_id);
            return false;
        });
    }

    void EnemyHandler::SpikeTarget(GW::Agent* target) {
        auto player = GW::Agents().GetPlayer();
        auto agents = GW::Agents().GetAgentArray();

        if (!player || !agents.valid())
            return;

        const auto nearbySq = GW::Constants::Range::Nearby * GW::Constants::Range::Nearby;

        std::vector<GW::Agent*> enemiesToSpike;
        for (auto agent : agents) {
            if (agent
                && !agent->GetIsDead()
                && agent->Allegiance == 3
                && !agent->GetIsSpawned()
                && agent->pos.SquaredDistanceTo(target->pos) < nearbySq) {

                enemiesToSpike.push_back(agent);
            }
        }

        switch (playerType) {
        case PlayerType::Vor:
            SpikeAsVoR(target);
            break;
        case PlayerType::ESurge:
        {
            auto loginNumber = GW::Agents().GetPlayer()->LoginNumber;
            int n = 1 + (loginNumber % 4);
            SpikeAsESurge(target, n);
            break;
        }
        case PlayerType::Rez:
            SpikeAsRez(target);
            break;
        }
    }

    void EnemyHandler::SpikeAsVoR(GW::Agent* target) {
        auto otherAdjacentEnemies = GetOtherEnemiesInRange(target, GW::Constants::Range::Adjacent);
        auto offTarget = otherAdjacentEnemies.size() > 0 ? otherAdjacentEnemies[0] : target;

        auto s = std::wstring(L"<c=#00ccff>Target</c>: ") + std::to_wstring(offTarget->Id) + ModelIdToString(offTarget->PlayerNumber);
        GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA3, s.c_str());

        auto vor = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Visions_of_Regret);
        if (vor.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Visions_of_Regret);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, target->Id);
                return;
            }
        }

        if (offTarget->GetIsHexed()) {
            auto shatter = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Shatter_Delusions);
            if (shatter.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Shatter_Delusions);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                    return;
                }
            }
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, target->Id);
                    return;
                }
            }
        }
        else if (offTarget->GetIsHexed() || offTarget->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                    return;
                }
            }
        }

        auto overload = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Overload);
        if (overload.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Overload);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                return;
            }
        }
    }

    void EnemyHandler::SpikeAsESurge(GW::Agent* target, unsigned int n) {
        auto otherAdjacentEnemies = GetOtherEnemiesInRange(target, GW::Constants::Range::Adjacent, [&](GW::Agent* a, GW::Agent* b) {
            if ((HasHighEnergy(a) && !HasHighEnergy(b)) || (!HasHighEnergy(a) && HasHighEnergy(b))) {
                return HasHighEnergy(a);
            }

            return a->Id < b->Id;
        });
        auto offTarget = otherAdjacentEnemies.size() > n
            ? otherAdjacentEnemies[n]
            : (otherAdjacentEnemies.size() > 0)
                ? otherAdjacentEnemies[(n % otherAdjacentEnemies.size())]
                : target;

        auto s = std::wstring(L"<c=#00ccff>Target</c>: ") + std::to_wstring(offTarget->Id) + ModelIdToString(offTarget->PlayerNumber);
        GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA3, s.c_str());

        auto esurge = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Energy_Surge);
        if (esurge.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Energy_Surge);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                return;
            }
        }

        if (offTarget->GetIsHexed()) {
            auto shatter = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Shatter_Delusions);
            if (shatter.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Shatter_Delusions);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                    return;
                }
            }
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, target->Id);
                    return;
                }
            }
        }
        else if (offTarget->GetIsHexed() || offTarget->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                    return;
                }
            }
        }

        auto overload = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Overload);
        if (overload.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Overload);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                return;
            }
        }
    }

    void EnemyHandler::SpikeAsRez(GW::Agent* target) {
        auto otherAdjacentEnemies = GetOtherEnemiesInRange(target, GW::Constants::Range::Adjacent, [&](GW::Agent* a, GW::Agent* b) {
            if ((HasHighAttackRate(a) && !HasHighAttackRate(b)) || (!HasHighAttackRate(a) && HasHighAttackRate(b))) {
                return HasHighAttackRate(a);
            }

            return a->Id < b->Id;
        });
        auto offTarget = otherAdjacentEnemies.size() > 0 ? otherAdjacentEnemies[0] : target;

        auto s = std::wstring(L"<c=#00ccff>Target</c>: ") + std::to_wstring(offTarget->Id) + ModelIdToString(offTarget->PlayerNumber);
        GW::Chat().WriteChat(GW::Channel::CHANNEL_GWCA3, s.c_str());

        if (HasHighAttackRate(target)) {
            auto wandering = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Wandering_Eye);
            if (wandering.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Wandering_Eye);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, target->Id);
                    return;
                }
            }
        }

        auto wandering = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Wandering_Eye);
        if (wandering.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Wandering_Eye);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                return;
            }
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, target->Id);
                    return;
                }
            }
        }
        else if (offTarget->GetIsHexed() || offTarget->GetIsEnchanted()) {
            auto signet = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Unnatural_Signet);
            if (signet.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unnatural_Signet);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, offTarget->Id);
                    return;
                }
            }
        }

        auto overload = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Overload);
        if (overload.GetRecharge() == 0) {
            auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Overload);
            if (slot != -1) {
                GW::Skillbarmgr().UseSkill(slot, target->Id);
                return;
            }
        }

        if (target->GetIsHexed()) {
            auto shatter = GW::Skillbar::GetPlayerSkillbar().GetSkillById(GW::Constants::SkillID::Shatter_Delusions);
            if (shatter.GetRecharge() == 0) {
                auto slot = GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Shatter_Delusions);
                if (slot != -1) {
                    GW::Skillbarmgr().UseSkill(slot, target->Id);
                    return;
                }
            }
        }
    }

    std::vector<GW::Agent*> EnemyHandler::GetOtherEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/) {
        std::vector<GW::Agent*> enemies;

        auto playerPos = GW::Agents().GetPlayer()->pos;
        auto rangeSq = range * range;
        for (auto agent : GW::Agents().GetAgentArray()) {
            if (agent
                && agent->Id != target->Id
                && !agent->GetIsDead()
                && agent->Allegiance == 3
                && !agent->GetIsSpawned()
                && agent->pos.SquaredDistanceTo(playerPos) < rangeSq) {

                enemies.push_back(agent);
            }
        }

        if (sort) {
            std::sort(enemies.begin(), enemies.end(), sort);
        }

        return enemies;
    }

    bool EnemyHandler::HasHighEnergy(GW::Agent* agent) {
        switch (agent->PlayerNumber) {
        case GW::Constants::ModelID::DoA::HeartTormentor:
        case GW::Constants::ModelID::DoA::MargoniteAnurDabi:
        case GW::Constants::ModelID::DoA::MargoniteAnurKaya:
        case GW::Constants::ModelID::DoA::MargoniteAnurKi:
        case GW::Constants::ModelID::DoA::MargoniteAnurSu:
        case GW::Constants::ModelID::DoA::MindTormentor:
        case GW::Constants::ModelID::DoA::SoulTormentor:
        case GW::Constants::ModelID::DoA::StygianHunger:
        case GW::Constants::ModelID::DoA::StygianLordEle:
        case GW::Constants::ModelID::DoA::StygianLordMesmer:
        case GW::Constants::ModelID::DoA::StygianLordMonk:
        case GW::Constants::ModelID::DoA::StygianLordNecro:
        case GW::Constants::ModelID::DoA::WaterTormentor:
        //case GW::Constants::ModelID::DoA::AnguishTitan
        //case GW::Constants::ModelID::DoA::TerrorwebDryder
        //case GW::Constants::ModelID::DoA::DreamRider
            return true;
        default:
            return false;
        }
    }

    bool EnemyHandler::HasHighAttackRate(GW::Agent* agent) {
        switch (agent->PlayerNumber) {
        case GW::Constants::ModelID::DoA::BlackBeastOfArgh:
        case GW::Constants::ModelID::DoA::EarthTormentor:
        case GW::Constants::ModelID::DoA::FleshTormentor:
        case GW::Constants::ModelID::DoA::MargoniteAnurMank:
        case GW::Constants::ModelID::DoA::MargoniteAnurRand:
        case GW::Constants::ModelID::DoA::MargoniteAnurRuk:
        case GW::Constants::ModelID::DoA::MargoniteAnurTuk:
        case GW::Constants::ModelID::DoA::MargoniteAnurVi:
        case GW::Constants::ModelID::DoA::SpiritTormentor:
        case GW::Constants::ModelID::DoA::StygianBrute:
        case GW::Constants::ModelID::DoA::StygianFiend:
        case GW::Constants::ModelID::DoA::StygianGolem:
        case GW::Constants::ModelID::DoA::StygianHorror:
        case GW::Constants::ModelID::DoA::StygianLordDerv:
        case GW::Constants::ModelID::DoA::StygianLordRanger:
            return true;
        default:
            return false;
        }
    }

    std::wstring EnemyHandler::ModelIdToString(int id) {
        switch (id) {
        case GW::Constants::ModelID::DoA::EarthTormentor: return L"Earth Tormentor";
        case GW::Constants::ModelID::DoA::FleshTormentor: return L"Flesh Tormentor";
        case GW::Constants::ModelID::DoA::SoulTormentor: return L"Soul Tormentor";
        case GW::Constants::ModelID::DoA::MindTormentor: return L"Mind Tormentor";
        case GW::Constants::ModelID::DoA::HeartTormentor: return L"Heart Tormentor";
        case GW::Constants::ModelID::DoA::WaterTormentor: return L"Water Tormentor";
        case GW::Constants::ModelID::DoA::SanityTormentor: return L"Sanity Tormentor";
        case GW::Constants::ModelID::DoA::SpiritTormentor: return L"Spirit Tormentor";
        default: return L"Unknown";
        }
    }
}
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

#include "../Utilities/SkillUtility.h"
#include "EnemyHandler.h"

namespace gbr::InProcess {
    using SkillUtility = Utilities::SkillUtility;

    EnemyHandler* EnemyHandler::instance = nullptr;

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

        auto loginNumber = GW::Agents().GetPlayer()->LoginNumber;
        auto charName = GW::Agents().GetPlayerNameByLoginNumber(loginNumber);

        int n = 0;
        if (charName == L"W T F Force") {
            n = 0;
        }
        else if (charName == L"Callisto Delta") {
            n = 1;
        }
        else if (charName == L"Bellatrix Beta") {
            n = 2;
        }
        else if (charName == L"Marianne Gamma") {
            n = 3;
        }

        if (type == PlayerType::Bonder) {
            return TRUE;
        }

        instance = new EnemyHandler(type, n);

        return TRUE;
    }

    EnemyHandler::EnemyHandler(PlayerType playerType, int esurgeNumber) : playerType(playerType), esurgeNumber(esurgeNumber) {
        static long long sleepUntil = 0;

        hookGuid = GW::Gamethread().AddPermanentCall([this]() {
            long long currentTime = GetTickCount();

            if ((sleepUntil - currentTime) > 0) {
                return;
            }

            sleepUntil = currentTime + 10;

            auto player = GW::Agents().GetPlayer();
            auto agents = GW::Agents().GetAgentArray();

            if (!player || player->GetIsDead() || !agents.valid())
                return;

            auto id = gbr::Shared::Commands::AggressiveMoveTo::GetTargetAgentId();
            if (id) {
                auto agent = agents[id];
                if (agent) {
                    if (!agent->GetIsDead()) {
                        if (agent->Allegiance == 3) {
                            // kill the enemy
                            if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                                GW::Agents().Move(player->pos); // stop moving
                                SpikeTarget(agent);
                            }
                            else {
                                GW::Agents().Move(agent->pos);
                            }

                            return;
                        }
                        else if (agent->IsNPC()) {
                            if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Adjacent) {
                                AcceptNextDialog();
                                GW::Agents().GoNPC(agent);
                                // after here we fall out of the if-else scopes and erase the target Id
                            }
                            else if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                                AcceptNextDialog();
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
                            if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Adjacent) {
                                // already at target, so fall through and set target Id to 0
                            }
                            if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
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
                    //else if (agent->IsPlayer()) {
                    //    // rez player
                    //    if (playerType == PlayerType::Rez) {
                    //        if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                    //            if (!RessurectTarget(agent))
                    //                return;
                    //            // after here we fall out of the if-else scopes and erase the target Id
                    //        }
                    //        else {
                    //            GW::Agents().Move(agent->pos);
                    //            return;
                    //        }
                    //    }
                    //}

                    gbr::Shared::Commands::AggressiveMoveTo::SetTargetAgentId(0);
                }
            }
        });
    }

    EnemyHandler::~EnemyHandler() {
        GW::Gamethread().RemovePermanentCall(hookGuid);
    }

    void EnemyHandler::JumpToTarget(GW::Agent* target) {
        if (!SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, target->Id)) {
            if (target->IsNPC())
                GW::Agents().GoNPC(target);
            else if (target->IsPlayer())
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
            if (ua.SkillId > 0 && ua.GetRecharge() == 0 && !GW::Skillbar::GetPlayerSkillbar().Casting)
                GW::Skillbarmgr().UseSkill(GW::Skillbarmgr().GetSkillSlot(GW::Constants::SkillID::Unyielding_Aura));

            return false;
        }
    }

    void EnemyHandler::AcceptNextDialog() {
        static bool queued = false;
        if (!queued) {
            queued = true;
            GW::StoC().AddSingleGameServerEvent<GW::Packet::StoC::P114_DialogButton>([](GW::Packet::StoC::P114_DialogButton* packet) {
                queued = false;
                GW::Agents().Dialog(packet->dialog_id);
                return false;
            });
        }
    }

    void EnemyHandler::SpikeTarget(GW::Agent* target) {
        auto overloadTargets = GetOtherEnemiesInRange(target, GW::Constants::SqrRange::Adjacent, [](GW::Agent* a, GW::Agent* b) {
            if ((a->Skill > 0 && b->Skill == 0) || (a->Skill == 0 && b->Skill > 0)) {
                return a->Skill > 0;
            }

            return a->Id < b->Id;
        });

        switch (playerType) {
        case PlayerType::Vor:
        {
            GW::Agent* vorOverloadTarget = overloadTargets.size() > 4 ? overloadTargets[4] : target;
            SpikeAsVoR(target, vorOverloadTarget);
            break;
        }
        case PlayerType::ESurge:
        {
            GW::Agent* esurgeOverloadTarget = overloadTargets.size() > esurgeNumber ? overloadTargets[esurgeNumber] : target;
            SpikeAsESurge(target, esurgeNumber, esurgeOverloadTarget);
            break;
        }
        case PlayerType::Rez:
            //SpikeAsRez(target);
            break;
        }
    }

    void EnemyHandler::SpikeAsVoR(GW::Agent* target, GW::Agent* overloadTarget) {
        if (target->HP < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Finish_Him, target->Id))
                return;
        }

        if (overloadTarget->HP < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Finish_Him, overloadTarget->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Visions_of_Regret, target->Id)) {
            GW::Chat().SendChat((L"VoR on " + GetAgentName(target)).c_str(), L'#');
            return;
        }

        if (overloadTarget->Skill > 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, overloadTarget->Id))
                return;
        }

        if (overloadTarget->GetIsHexed()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Shatter_Delusions, overloadTarget->Id))
                return;
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, target->Id))
                return;
        }
        else if (overloadTarget->GetIsHexed() || overloadTarget->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, overloadTarget->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, overloadTarget->Id))
            return;
    }

    void EnemyHandler::SpikeAsESurge(GW::Agent* target, unsigned int n, GW::Agent* overloadTarget) {
        auto esurgeTargets = GetEnemiesInRange(target, GW::Constants::SqrRange::Adjacent, [](GW::Agent* a, GW::Agent* b) {
            if ((HasHighEnergy(a) && !HasHighEnergy(b)) || (!HasHighEnergy(a) && HasHighEnergy(b))) {
                return HasHighEnergy(a);
            }

            return a->Id < b->Id;
        });
        auto esurgeTarget = esurgeTargets.size() > n
            ? esurgeTargets[n]
            : (esurgeTargets.size() > 0)
                ? esurgeTargets[(n % esurgeTargets.size())]
                : target;

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Energy_Surge, esurgeTarget->Id)) {
            GW::Chat().SendChat((L"ESurge on " + GetAgentName(esurgeTarget)).c_str(), L'#');
            return;
        }

        if (overloadTarget->Skill > 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, overloadTarget->Id))
                return;
        }

        if (overloadTarget->GetIsHexed()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Shatter_Delusions, overloadTarget->Id))
                return;
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, target->Id))
                return;
        }
        else if (overloadTarget->GetIsHexed() || overloadTarget->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, overloadTarget->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, overloadTarget->Id))
            return;
    }

    void EnemyHandler::SpikeAsRez(GW::Agent* target) {
        auto otherAdjacentEnemies = GetOtherEnemiesInRange(target, GW::Constants::SqrRange::Adjacent, [&](GW::Agent* a, GW::Agent* b) {
            if ((HasHighAttackRate(a) && !HasHighAttackRate(b)) || (!HasHighAttackRate(a) && HasHighAttackRate(b))) {
                return HasHighAttackRate(a);
            }

            return a->Id < b->Id;
        });
        auto offTarget = otherAdjacentEnemies.size() > 0 ? otherAdjacentEnemies[0] : target;

        if (HasHighAttackRate(target)) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Wandering_Eye, target->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Wandering_Eye, offTarget->Id))
            return;

        if (target->Skill > 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, target->Id))
                return;
        }

        if (offTarget->Skill > 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, offTarget->Id))
                return;
        }

        if (target->GetIsHexed() || target->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, target->Id))
                return;
        }
        else if (offTarget->GetIsHexed() || offTarget->GetIsEnchanted()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, offTarget->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, target->Id))
            return;

        if (target->GetIsHexed()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Shatter_Delusions, target->Id))
                return;
        }
    }

    std::vector<GW::Agent*> EnemyHandler::GetOtherEnemiesInRange(GW::Agent* target, float rangeSq, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/) {
        std::vector<GW::Agent*> enemies;

        auto targetPos = target->pos;
        for (auto agent : GW::Agents().GetAgentArray()) {
            if (agent
                && agent->Id != target->Id
                && !agent->GetIsDead()
                && agent->Allegiance == 3
                && !agent->GetIsSpawned()
                && agent->pos.SquaredDistanceTo(targetPos) < rangeSq) {

                enemies.push_back(agent);
            }
        }

        if (sort) {
            std::sort(enemies.begin(), enemies.end(), sort);
        }

        return enemies;
    }

    std::vector<GW::Agent*> EnemyHandler::GetEnemiesInRange(GW::Agent* target, float rangeSq, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/) {
        std::vector<GW::Agent*> enemies;

        auto targetPos = target->pos;
        for (auto agent : GW::Agents().GetAgentArray()) {
            if (agent
                && !agent->GetIsDead()
                && agent->Allegiance == 3
                && !agent->GetIsSpawned()
                && agent->pos.SquaredDistanceTo(targetPos) < rangeSq) {

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
        case GW::Constants::ModelID::DoA::MargoniteAnurDabi:
        case GW::Constants::ModelID::DoA::MargoniteAnurKaya:
        case GW::Constants::ModelID::DoA::MargoniteAnurKi:
        case GW::Constants::ModelID::DoA::MargoniteAnurSu:
        case GW::Constants::ModelID::DoA::MindTormentor:
        case GW::Constants::ModelID::DoA::SoulTormentor:
        case GW::Constants::ModelID::DoA::HeartTormentor:
        case GW::Constants::ModelID::DoA::WaterTormentor:
        case GW::Constants::ModelID::DoA::VeilMindTormentor:
        case GW::Constants::ModelID::DoA::VeilSoulTormentor:
        case GW::Constants::ModelID::DoA::VeilHeartTormentor:
        case GW::Constants::ModelID::DoA::VeilWaterTormentor:
        case GW::Constants::ModelID::DoA::StygianHunger:
        case GW::Constants::ModelID::DoA::StygianLordEle:
        case GW::Constants::ModelID::DoA::StygianLordMesmer:
        case GW::Constants::ModelID::DoA::StygianLordMonk:
        case GW::Constants::ModelID::DoA::StygianLordNecro:
        case GW::Constants::ModelID::DoA::AnguishTitan:
        case GW::Constants::ModelID::DoA::DespairTitan:
        case GW::Constants::ModelID::DoA::TorturewebDryder:
        case GW::Constants::ModelID::DoA::GreaterDreamRider:
            return true;
        default:
            return false;
        }
    }

    bool EnemyHandler::HasHighAttackRate(GW::Agent* agent) {
        switch (agent->PlayerNumber) {
        case GW::Constants::ModelID::DoA::BlackBeastOfArgh:
        case GW::Constants::ModelID::DoA::MargoniteAnurMank:
        case GW::Constants::ModelID::DoA::MargoniteAnurRund:
        case GW::Constants::ModelID::DoA::MargoniteAnurRuk:
        case GW::Constants::ModelID::DoA::MargoniteAnurTuk:
        case GW::Constants::ModelID::DoA::MargoniteAnurVu:
        case GW::Constants::ModelID::DoA::EarthTormentor:
        case GW::Constants::ModelID::DoA::FleshTormentor:
        case GW::Constants::ModelID::DoA::SpiritTormentor:
        case GW::Constants::ModelID::DoA::SanityTormentor:
        case GW::Constants::ModelID::DoA::VeilEarthTormentor:
        case GW::Constants::ModelID::DoA::VeilFleshTormentor:
        case GW::Constants::ModelID::DoA::VeilSpiritTormentor:
        case GW::Constants::ModelID::DoA::VeilSanityTormentor:
        case GW::Constants::ModelID::DoA::StygianBrute:
        case GW::Constants::ModelID::DoA::StygianFiend:
        case GW::Constants::ModelID::DoA::StygianGolem:
        case GW::Constants::ModelID::DoA::StygianHorror:
        case GW::Constants::ModelID::DoA::StygianLordDerv:
        case GW::Constants::ModelID::DoA::StygianLordRanger:
        case GW::Constants::ModelID::DoA::FuryTitan:
        //case GW::Constants::ModelID::DoA::DementiaTitan:
            return true;
        default:
            return false;
        }
    }

    std::wstring EnemyHandler::GetAgentName(GW::Agent* agent) {
        switch (agent->PlayerNumber) {
        case GW::Constants::ModelID::DoA::MargoniteAnurDabi: return L"MargoniteAnurDabi";
        case GW::Constants::ModelID::DoA::MargoniteAnurKaya: return L"MargoniteAnurKaya";
        case GW::Constants::ModelID::DoA::MargoniteAnurKi: return L"MargoniteAnurKi";
        case GW::Constants::ModelID::DoA::MargoniteAnurMank: return L"MargoniteAnurMank";
        case GW::Constants::ModelID::DoA::MargoniteAnurRuk: return L"MargoniteAnurRuk";
        case GW::Constants::ModelID::DoA::MargoniteAnurRund: return L"MargoniteAnurRund";
        case GW::Constants::ModelID::DoA::MargoniteAnurSu: return L"MargoniteAnurSu";
        case GW::Constants::ModelID::DoA::MargoniteAnurTuk: return L"MargoniteAnurTuk";
        case GW::Constants::ModelID::DoA::MargoniteAnurVu: return L"MargoniteAnurVu";
        case GW::Constants::ModelID::DoA::AnguishTitan: return L"AnguishTitan";
        case GW::Constants::ModelID::DoA::DespairTitan: return L"DespairTitan";
        case GW::Constants::ModelID::DoA::FuryTitan: return L"FuryTitan";
        default: return std::to_wstring(agent->Id).c_str();
        }
    }
}
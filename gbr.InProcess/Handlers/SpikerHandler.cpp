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
#include "SpikerHandler.h"

namespace gbr::InProcess {
    using SkillUtility = Utilities::SkillUtility;

    SpikerHandler::SpikerHandler(Utilities::PlayerType playerType) : playerType(playerType) {
        hookGuid = GW::Gamethread().AddPermanentCall([this]() {
            Tick();
        });
    }

    SpikerHandler::~SpikerHandler() {
        GW::Gamethread().RemovePermanentCall(hookGuid);
    }

    void SpikerHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

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
                        if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Nearby) {
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

                gbr::Shared::Commands::AggressiveMoveTo::SetTargetAgentId(0);
            }
        }
    }

    void SpikerHandler::SpikeTarget(GW::Agent* target) {
        auto overloadTargets = GetOtherEnemiesInRange(target, GW::Constants::SqrRange::Adjacent, [](GW::Agent* a, GW::Agent* b) {
            if ((a->Skill > 0 && b->Skill == 0) || (a->Skill == 0 && b->Skill > 0)) {
                return a->Skill > 0;
            }

            return a->Id < b->Id;
        });

        switch (playerType) {
        case Utilities::PlayerType::VoR:
            SpikeAsVoR(target, overloadTargets.size() > 4 ? overloadTargets[4] : target);
            break;
        case Utilities::PlayerType::ESurge1:
            SpikeAsESurge(target, overloadTargets.size() > 0 ? overloadTargets[0] : target);
            break;
        case Utilities::PlayerType::ESurge2:
            SpikeAsESurge(target, overloadTargets.size() > 1 ? overloadTargets[1] : target);
            break;
        case Utilities::PlayerType::ESurge3:
            SpikeAsESurge(target, overloadTargets.size() > 2 ? overloadTargets[2] : target);
            break;
        case Utilities::PlayerType::ESurge4:
            SpikeAsESurge(target, overloadTargets.size() > 3 ? overloadTargets[3] : target);
            break;
        }
    }

    void SpikerHandler::SpikeAsVoR(GW::Agent* target, GW::Agent* overloadTarget) {
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

    void SpikerHandler::SpikeAsESurge(GW::Agent* target, GW::Agent* overloadTarget) {
        auto esurgeTargets = GetEnemiesInRange(target, GW::Constants::SqrRange::Nearby, [](GW::Agent* a, GW::Agent* b) {
            if ((HasHighEnergy(a) && !HasHighEnergy(b)) || (!HasHighEnergy(a) && HasHighEnergy(b))) {
                return HasHighEnergy(a);
            }

            return a->Id < b->Id;
        });

        GW::Agent* esurgeTarget;
        switch (playerType) {
        case Utilities::PlayerType::ESurge1:
            esurgeTarget = esurgeTargets.size() > 0 ? esurgeTargets[0] : target;
            break;
        case Utilities::PlayerType::ESurge2:
            esurgeTarget = esurgeTargets.size() > 1 ? esurgeTargets[1] : target;
            break;
        case Utilities::PlayerType::ESurge3:
            esurgeTarget = esurgeTargets.size() > 2 ? esurgeTargets[2] : target;
            break;
        case Utilities::PlayerType::ESurge4:
            esurgeTarget = esurgeTargets.size() > 3 ? esurgeTargets[3] : target;
            break;
        default:
            esurgeTarget = target;
            break;
        }

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


    void SpikerHandler::JumpToTarget(GW::Agent* target) {
        if (!SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, target->Id)) {
            if (target->IsNPC())
                GW::Agents().GoNPC(target);
            else if (target->IsPlayer())
                GW::Agents().Move(target->pos);
        }
    }

    void SpikerHandler::AcceptNextDialog() {
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

    std::vector<GW::Agent*> SpikerHandler::GetOtherEnemiesInRange(GW::Agent* target, float rangeSq, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/) {
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

    std::vector<GW::Agent*> SpikerHandler::GetEnemiesInRange(GW::Agent* target, float rangeSq, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/) {
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

    bool SpikerHandler::HasHighEnergy(GW::Agent* agent) {
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

    bool SpikerHandler::HasHighAttackRate(GW::Agent* agent) {
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

    std::wstring SpikerHandler::GetAgentName(GW::Agent* agent) {
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
        //case GW::Constants::ModelID::DoA::DementiaTitan: return L"DementiaTitan";
        default: return std::to_wstring(agent->Id).c_str();
        }
    }
}
#include <algorithm>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <gbr.Shared/Commands/InteractAgent.h>
#include <gbr.Shared/Commands/MoveTo.h>
#include <gbr.Shared/Commands/PlaceSpirit.h>

#include "../Utilities/SkillUtility.h"
#include "SpikerHandler.h"

namespace gbr::InProcess {
    using SkillUtility = Utilities::SkillUtility;

    SpikerHandler::SpikerHandler(Utilities::PlayerType playerType) : playerType(playerType) {
        hookId = GW::Gamethread().AddPermanentCall([this]() {
            Tick();
        });

        gameServerHookId = GW::StoC().AddGameServerEvent<GW::Packet::StoC::P114_DialogButton>([](GW::Packet::StoC::P114_DialogButton* packet) {
            GW::Agents().Dialog(packet->dialog_id);
            return false;
        });
    }

    SpikerHandler::~SpikerHandler() {
        GW::Gamethread().RemovePermanentCall(hookId);
        GW::StoC().RemoveGameServerEvent<GW::Packet::StoC::P114_DialogButton>(gameServerHookId);
    }

    void SpikerHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

        sleepUntil = currentTime + 10;

        if (!GW::Map().IsMapLoaded()) {
            gbr::Shared::Commands::MoveTo::ClearPos();
            sleepUntil += 1000;
            return;
        }

        auto player = GW::Agents().GetPlayer();
        auto agents = GW::Agents().GetAgentArray();
        auto partyInfo = GW::Partymgr().GetPartyInfo();

        if (!player || player->GetIsDead() || !agents.valid() || !partyInfo || !partyInfo->players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
            return;

        auto players = partyInfo->players;

        auto interactAgentId = gbr::Shared::Commands::InteractAgent::GetAgentId();

        GW::Agent* emo = nullptr;
        GW::Agent* tank = nullptr;
        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);

            if (agent) {
                if (agent->Primary == (BYTE)GW::Constants::Profession::Elementalist && agent->Secondary == (BYTE)GW::Constants::Profession::Monk) {
                    emo = agent;
                }
                else if (agent->Primary == (BYTE)GW::Constants::Profession::Ranger || agent->Primary == (BYTE)GW::Constants::Profession::Assassin) {
                    tank = agent;
                }
            }
        }

        auto movePos = gbr::Shared::Commands::MoveTo::GetPos();
        if (movePos.HasValue()) {
            if (interactAgentId.HasValue()) {
                auto agent = agents[interactAgentId.Value()];
                if (!agent || agent->GetIsDead() || !agent->IsNPC()) {
                    if (emo) {
                        auto distanceToEmo = emo->pos.SquaredDistanceTo(player->pos);

                        if (distanceToEmo < GW::Constants::SqrRange::Spellcast) {
                            if (distanceToEmo > GW::Constants::SqrRange::Area && SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, emo->Id))
                                return;
                        }
                        else {
                            for (auto p : players) {
                                auto agentId = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);

                                if (tank && agentId == tank->Id)
                                    continue;

                                if (agentId == emo->Id)
                                    continue;

                                if (agentId == player->Id)
                                    continue;

                                auto agent = GW::Agents().GetAgentByID(agentId);
                                auto agentToEmo = emo->pos.SquaredDistanceTo(agent->pos);

                                if (distanceToEmo - agentToEmo > GW::Constants::SqrRange::Nearby) {
                                    if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, agentId))
                                        return;
                                }
                            }
                        }
                    }

                    return;
                }
            }
            else {
                return;
            }
        }

        if (interactAgentId.HasValue() && interactAgentId.Value() != player->Id) {
            auto agent = agents[interactAgentId.Value()];
            if (agent) {
                if (!agent->GetIsDead()) {
                    if (agent->Allegiance == 3) {
                        // kill the enemy
                        if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                            SpikeTarget(agent);
                        }
                        else {
                            auto agentToPlayer = player->pos - agent->pos;
                            auto newPos = agent->pos + (agentToPlayer.Normalized() * (GW::Constants::Range::Spellcast - 10.0f));

                            GW::Agents().Move(newPos.x, newPos.y);
                        }

                        return;
                    }
                    else if (agent->IsNPC()) {
                        GW::Agents().GoNPC(agent);
                        return;
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
                else if (agent->IsPlayer()) {
                    if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                        GW::Agents().Move(player->pos); // stop moving
                        RezTarget(agent);
                    }
                    else {
                        GW::Agents().Move(agent->pos);
                        return;
                    }
                }

                gbr::Shared::Commands::InteractAgent::ClearAgentId();
            }
        }

        auto eternalAura = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Eternal_Aura);
        if (eternalAura.SkillId == 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Eternal_Aura, player->Id))
                return;
        }
    }

    void SpikerHandler::SpikeTarget(GW::Agent* target) {
        auto overloadTargets = GetOtherEnemiesInRange(target, GW::Constants::SqrRange::Adjacent, [](GW::Agent* a, GW::Agent* b) {
            if ((a->Skill > 0) != (b->Skill > 0)) {
                return a->Skill > 0;
            }

            return a->Id < b->Id;
        });

        auto mistrustTargets = GetEnemiesInRange(
            target,
            GW::Constants::SqrRange::Nearby,
            [](GW::Agent* a, GW::Agent* b) { return a->Id < b->Id; },
            [](GW::Agent* a) { return UsesSpellsOnAllies(a); });

        if (overloadTargets.size() == 0) {
            SpikeSingleTarget(target);
        }
        else {
            switch (playerType) {
            case Utilities::PlayerType::VoR:
                SpikeAsVoR(target, overloadTargets.size() > 4 ? overloadTargets[4] : target, mistrustTargets.size() > 4 ? mistrustTargets[4] : nullptr);
                break;
            case Utilities::PlayerType::ESurge1:
                SpikeAsESurge(target, overloadTargets.size() > 0 ? overloadTargets[0] : target, mistrustTargets.size() > 0 ? mistrustTargets[0] : nullptr);
                break;
            case Utilities::PlayerType::ESurge2:
                SpikeAsESurge(target, overloadTargets.size() > 1 ? overloadTargets[1] : target, mistrustTargets.size() > 1 ? mistrustTargets[1] : nullptr);
                break;
            case Utilities::PlayerType::ESurge3:
                SpikeAsESurge(target, overloadTargets.size() > 2 ? overloadTargets[2] : target, mistrustTargets.size() > 2 ? mistrustTargets[2] : nullptr);
                break;
            case Utilities::PlayerType::ESurge4:
                SpikeAsESurge(target, overloadTargets.size() > 3 ? overloadTargets[3] : target, mistrustTargets.size() > 3 ? mistrustTargets[3] : nullptr);
                break;
            }
        }
    }

    void SpikerHandler::SpikeSingleTarget(GW::Agent* target) {
        if (target->HP > 0.3f) {
            if (HasHighAttackRate(target)) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Empathy, target->Id))
                    return;
            }

            if (HasHighEnergy(target)) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Backfire, target->Id))
                    return;
            }
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Visions_of_Regret, target->Id))
            return;

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Unnatural_Signet, target->Id))
            return;

        if ((target->GetIsEnchanted() || target->GetIsConditioned()) && SkillUtility::TryUseSkill(GW::Constants::SkillID::Necrosis, target->Id))
            return;

        auto player = GW::Agents().GetPlayer();
        if (player && player->Energy * player->MaxEnergy > 20) {
            if (target->HP < 0.5f && !target->GetIsDeepWounded()) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Finish_Him, target->Id))
                    return;
            }

            if (target->Skill > 0) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Overload, target->Id))
                    return;
            }

            if (target->Energy > 0.1f && SkillUtility::TryUseSkill(GW::Constants::SkillID::Energy_Surge, target->Id))
                return;

            if (target->GetIsHexed()) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Shatter_Delusions, target->Id))
                    return;
            }
        }
    }

    void SpikerHandler::SpikeAsVoR(GW::Agent* target, GW::Agent* overloadTarget, GW::Agent* mistrustTarget) {
        if (target->HP < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Finish_Him, target->Id))
                return;
        }

        if (overloadTarget->HP < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Finish_Him, overloadTarget->Id))
                return;
        }

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Visions_of_Regret, target->Id))
            return;

        if (mistrustTarget && SkillUtility::TryUseSkill(GW::Constants::SkillID::Mistrust, mistrustTarget->Id))
            return;

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

    void SpikerHandler::SpikeAsESurge(GW::Agent* target, GW::Agent* overloadTarget, GW::Agent* mistrustTarget) {
        auto esurgeTargets = GetEnemiesInRange(target, GW::Constants::SqrRange::Nearby, [](GW::Agent* a, GW::Agent* b) {
            if (HasHighEnergy(a) != HasHighEnergy(b)) {
                return HasHighEnergy(a);
            }

            return a->Id < b->Id;
        });
        auto wanderingTargets = GetEnemiesInRange(
            target,
            GW::Constants::SqrRange::Nearby,
            [](GW::Agent* a, GW::Agent* b) { return a->Id < b->Id; },
            [](GW::Agent* a) { return HasHighAttackRate(a); });

        GW::Agent* esurgeTarget;
        GW::Agent* wanderingTarget = nullptr;
        switch (playerType) {
        case Utilities::PlayerType::ESurge1:
            esurgeTarget = esurgeTargets.size() > 0 ? esurgeTargets[0] : target;
            wanderingTarget = wanderingTargets.size() > 0 ? wanderingTargets[0] : nullptr;
            break;
        case Utilities::PlayerType::ESurge2:
            esurgeTarget = esurgeTargets.size() > 1 ? esurgeTargets[1] : target;
            wanderingTarget = wanderingTargets.size() > 1 ? wanderingTargets[1] : nullptr;
            break;
        case Utilities::PlayerType::ESurge3:
            esurgeTarget = esurgeTargets.size() > 2 ? esurgeTargets[2] : target;
            wanderingTarget = wanderingTargets.size() > 2 ? wanderingTargets[2] : nullptr;
            break;
        case Utilities::PlayerType::ESurge4:
            esurgeTarget = esurgeTargets.size() > 3 ? esurgeTargets[3] : target;
            wanderingTarget = wanderingTargets.size() > 3 ? wanderingTargets[3] : nullptr;
            break;
        default:
            esurgeTarget = target;
            break;
        }

        //if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Mark_of_Pain, target->Id))
        //    return;

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Energy_Surge, esurgeTarget->Id))
            return;

        if (wanderingTarget && SkillUtility::TryUseSkill(GW::Constants::SkillID::Wandering_Eye, wanderingTarget->Id))
            return;

        if (mistrustTarget && SkillUtility::TryUseSkill(GW::Constants::SkillID::Mistrust, mistrustTarget->Id))
            return;

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

    void SpikerHandler::RezTarget(GW::Agent* target) {
        SkillUtility::TryUseSkill(GW::Constants::SkillID::Rebirth, target->Id);
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

    std::vector<GW::Agent*> SpikerHandler::GetEnemiesInRange(GW::Agent* target, float rangeSq, std::function<bool(GW::Agent*, GW::Agent*)> sort /*= nullptr*/, std::function<bool(GW::Agent*)> predicate /*= nullptr*/) {
        std::vector<GW::Agent*> enemies;

        auto targetPos = target->pos;
        for (auto agent : GW::Agents().GetAgentArray()) {
            if (agent
                && !agent->GetIsDead()
                && agent->Allegiance == 3
                && !agent->GetIsSpawned()
                && agent->pos.SquaredDistanceTo(targetPos) < rangeSq) {

                if (!predicate || predicate(agent))
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
        case GW::Constants::ModelID::DoA::RageTitan:
        case GW::Constants::ModelID::DoA::RageTitan2:
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
        case GW::Constants::ModelID::DoA::DementiaTitan:
        case GW::Constants::ModelID::DoA::DementiaTitan2:
            return true;
        default:
            return false;
        }
    }

    bool SpikerHandler::UsesSpellsOnAllies(GW::Agent* agent) {
        switch (agent->PlayerNumber) {
        case GW::Constants::ModelID::DoA::MargoniteAnurDabi:
        case GW::Constants::ModelID::DoA::MargoniteAnurKaya:
        case GW::Constants::ModelID::DoA::MargoniteAnurSu:
        case GW::Constants::ModelID::DoA::MindTormentor:
        case GW::Constants::ModelID::DoA::SoulTormentor:
        case GW::Constants::ModelID::DoA::WaterTormentor:
        case GW::Constants::ModelID::DoA::VeilMindTormentor:
        case GW::Constants::ModelID::DoA::VeilSoulTormentor:
        case GW::Constants::ModelID::DoA::VeilWaterTormentor:
        case GW::Constants::ModelID::DoA::StygianLordEle:
        case GW::Constants::ModelID::DoA::StygianLordMesmer:
        case GW::Constants::ModelID::DoA::StygianLordNecro:
        case GW::Constants::ModelID::DoA::AnguishTitan:
        case GW::Constants::ModelID::DoA::DespairTitan:
        case GW::Constants::ModelID::DoA::RageTitan:
        case GW::Constants::ModelID::DoA::RageTitan2:
        case GW::Constants::ModelID::DoA::TorturewebDryder:
        case GW::Constants::ModelID::DoA::GreaterDreamRider:
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
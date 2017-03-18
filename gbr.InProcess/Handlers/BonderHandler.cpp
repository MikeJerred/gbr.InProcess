#include <GWCA/GWCA.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Utilities/Maybe.h>
#include <gbr.Shared/Commands/PlaceSpirit.h>
#include <gbr.Shared/Commands/MoveTo.h>

#include "../Utilities/LogUtility.h"
#include "../Utilities/SkillUtility.h"
#include "BonderHandler.h"

namespace gbr::InProcess {
    using LogUtility = Utilities::LogUtility;
    using SkillUtility = Utilities::SkillUtility;

    BonderHandler::BonderHandler(Utilities::PlayerType playerType) : playerType(playerType) {
        hookId = GW::Gamethread().AddPermanentCall([this]() {
            Tick();
        });
    }

    BonderHandler::~BonderHandler() {
        GW::Gamethread().RemovePermanentCall(hookId);
    }

    void BonderHandler::Tick() {
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

        if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return;

        auto player = GW::Agents().GetPlayer();
        auto agents = GW::Agents().GetAgentArray();
        auto partyInfo = GW::Partymgr().GetPartyInfo();

        if (!player || player->GetIsDead() || !agents.valid() || !partyInfo || !partyInfo->players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
            return;

        auto players = partyInfo->players;
        if (!GW::Agents().GetPlayerArray().valid())
            return;

        LogUtility::Log(L"Starting valid cycle");

        auto moveToPos = gbr::Shared::Commands::MoveTo::GetPos();

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ether_Renewal, player->Id))
            return;


        auto pbond = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Protective_Bond);
        if (pbond.SkillId == 0) {
            LogUtility::Log(L"Prot on self");
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, player->Id))
                return;
        }

        auto balth = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Balthazars_Spirit);
        if (balth.SkillId == 0) {
            LogUtility::Log(L"Balth on self");
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Balthazars_Spirit, player->Id))
                return;
        }

        LogUtility::Log(L"We have all bonds on self!");

        if (player->Energy * player->MaxEnergy < 10) {
            LogUtility::Log(L"Low energy");
            auto buffs = GW::Effects().GetPlayerBuffArray();
            if (buffs.valid()) {
                for (auto buff : buffs) {
                    if (buff.TargetAgentId != player->Id) {
                        LogUtility::Log(L"Dropping a buff");
                        GW::Effects().DropBuff(buff.BuffId);
                        sleepUntil += 1000;
                        return;
                    }
                }
            }
        }

        LogUtility::Log(L"We have at least a little energy!");

        if (player->Energy < 0.5f || player->HP < 0.45f) {
            auto ether = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Ether_Renewal);

            if (ether.SkillId > 0) {
                if (moveToPos.HasValue()) {
                    LogUtility::Log(L"Try to EE for energy and to help movement");

                    auto bestDistanceToPos = player->pos.SquaredDistanceTo(moveToPos.Value());
                    GW::Agent* bestEETarget = nullptr;

                    for (auto p : players) {
                        auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
                        auto agent = GW::Agents().GetAgentByID(id);
                        auto distanceToPos = agent->pos.SquaredDistanceTo(moveToPos.Value());
                        if (agent
                            && !agent->GetIsDead()
                            && agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast
                            && distanceToPos < bestDistanceToPos) {

                            bestEETarget = agent;
                            bestDistanceToPos = distanceToPos;
                        }
                    }

                    if (bestEETarget) {
                        LogUtility::Log(L"Found a suitable EE target!");
                        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, bestEETarget->Id))
                            return;
                    }
                }

                LogUtility::Log(L"Spam for energy & HP");
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                    return;

                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                    return;
            }
            else if (player->HP < 0.45f) {
                LogUtility::Log(L"No ether but we are gonna die! Use spirit bond");
                SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id);
            }

            return;
        }

        LogUtility::Log(L"We have decent energy and HP!");

        if (moveToPos.HasValue()) {
            LogUtility::Log(L"Try to EE to help movement");

            auto distanceToPos = player->pos.SquaredDistanceTo(moveToPos.Value());

            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
                auto agent = GW::Agents().GetAgentByID(id);
                if (agent
                    && !agent->GetIsDead()
                    && agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast
                    && agent->pos.SquaredDistanceTo(moveToPos.Value()) < distanceToPos) {

                    LogUtility::Log(L"Found a suitable EE target!");
                    if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, agent->Id))
                        return;
                }
            }

        }

        GW::Agent* tank = nullptr;
        GW::Agent* monk = nullptr;
        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);
            if (agent && !agent->GetIsDead()) {
                if (agent->Primary == (BYTE)GW::Constants::Profession::Ranger || agent->Primary == (BYTE)GW::Constants::Profession::Assassin) {
                    LogUtility::Log(L"tank found");
                    tank = agent;
                }
                else if (agent->Primary == (BYTE)GW::Constants::Profession::Monk) {
                    LogUtility::Log(L"monk found");
                    monk = agent;
                }
            }
        }

        LogUtility::Log(L"We found the tank!");

        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);

            if (id == player->Id)
                continue;

            auto agent = GW::Agents().GetAgentByID(id);

            if (!agent || agent->GetIsDead())
                continue;

            auto distance = agent->pos.SquaredDistanceTo(player->pos);

            if ((distance < GW::Constants::SqrRange::Earshot)
                || (distance < GW::Constants::SqrRange::Spellcast 
                    && (!tank || id != tank->Id))) {

                LogUtility::Log(L"Seeing if other player needs to be healed");
                if (agent->HP < 0.5f && SkillUtility::TryUseSkill(GW::Constants::SkillID::Infuse_Health, agent->Id))
                    return;

                if (agent->HP < 0.7f && SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, agent->Id))
                    return;
            }
        }

        LogUtility::Log(L"We don't need to heal anyone!");

        LogUtility::Log(L"Check we have buffs up...");
        auto buffs = GW::Effects().GetPlayerBuffArray();
        if (buffs.valid()) {
            std::vector<GW::Buff> protBonds;
            std::vector<GW::Buff> lifeBonds;
            std::vector<GW::Buff> balthBonds;

            for (auto buff : buffs) {
                switch (buff.SkillId) {
                case (DWORD)GW::Constants::SkillID::Protective_Bond:
                    protBonds.push_back(buff);
                    break;
                case (DWORD)GW::Constants::SkillID::Life_Bond:
                    lifeBonds.push_back(buff);
                    break;
                case (DWORD)GW::Constants::SkillID::Balthazars_Spirit:
                    balthBonds.push_back(buff);
                    break;
                }
            }

            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);

                if (id == player->Id)
                    continue;

                auto agent = GW::Agents().GetAgentByID(id);
                if (!agent || agent->GetIsDead())
                    continue;

                if (tank && tank->Id == id) {
                    LogUtility::Log(L"Ensure tank is bonded");
                    if (!HasABond(id, lifeBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using life bond on tank");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Life_Bond, id))
                                return;
                        }
                    }

                    if (!HasABond(id, balthBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using balth on tank");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Balthazars_Spirit, id))
                                return;
                        }
                    }

                    if (!HasABond(id, protBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using prot bond on tank");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, id))
                                return;
                        }
                    }
                }
                else if (monk && monk->Id == id) {
                    LogUtility::Log(L"Ensure monk is bonded");
                    if (!HasABond(id, balthBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using balth on monk");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Balthazars_Spirit, id))
                                return;
                        }
                    }

                    if (!HasABond(id, protBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using prot bond on monk");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, id))
                                return;
                        }
                    }
                }
                else if (!HasABond(id, protBonds)) {
                    LogUtility::Log(L"Bonding others");
                    if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, id))
                            return;
                    }
                    else {
                        GW::Agents().Move(agent->pos);
                        return;
                    }
                }
            }
        }

        // ensure the emo doesn't get stuck if he stops moving (since when not moving he spams spirit bond + burning speed)
        if (moveToPos.HasValue()) {
            LogUtility::Log(L"Ensure we are moving...");
            GW::Agents().Move(moveToPos.Value());

            return;
        }

        if (!player->GetIsMoving() && player->Energy < 0.9f) {
            LogUtility::Log(L"Nothing to do so spam skills for energy & HP");
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                return;

            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                return;
        }

        LogUtility::Log(L"Nothing to do but we are moving are have full energy so don't spam");
    }

    bool BonderHandler::HasABond(DWORD agentId, std::vector<GW::Buff> bonds) {
        for (auto bond : bonds) {
            if (bond.TargetAgentId == agentId)
                return true;
        }

        return false;
    }
}
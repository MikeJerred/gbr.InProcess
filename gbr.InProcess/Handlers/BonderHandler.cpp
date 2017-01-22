#include <GWCA/GWCA.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Utilities/Maybe.h>
#include <gbr.Shared/Commands/AggressiveMoveTo.h>
#include <gbr.Shared/Commands/MoveTo.h>

#include "../Utilities/SkillUtility.h"
#include "BonderHandler.h"

namespace gbr::InProcess {
    using SkillUtility = Utilities::SkillUtility;

    BonderHandler::BonderHandler(Utilities::PlayerType playerType) : playerType(playerType) {
        if (playerType == Utilities::PlayerType::Monk) {
            hookGuid = GW::Gamethread().AddPermanentCall([this]() {
                TickMonk();
            });
        }
        else if (playerType == Utilities::PlayerType::Emo) {
            hookGuid = GW::Gamethread().AddPermanentCall([this]() {
                TickEmo();
            });
        }
    }

    BonderHandler::~BonderHandler() {
        GW::Gamethread().RemovePermanentCall(hookGuid);
    }

    void BonderHandler::TickMonk() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

        sleepUntil = currentTime + 10;

        auto player = GW::Agents().GetPlayer();
        auto partyInfo = GW::Partymgr().GetPartyInfo();
        if (!player || player->GetIsDead() || !partyInfo || !partyInfo->players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
            return;

        auto players = partyInfo->players;

        auto blessedAura = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Blessed_Aura);
        if (blessedAura.SkillId == 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Blessed_Aura, player->Id))
                return;
        }

        auto spiritPos = gbr::Shared::Commands::AggressiveMoveTo::GetSpiritPos();
        if (spiritPos != GW::Maybe<GW::GamePos>::Nothing()) {
            if (player->pos.SquaredDistanceTo(spiritPos.Value()) < GW::Constants::SqrRange::Adjacent) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Edge_of_Extinction, 0))
                    return;

                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Winnowing, 0)) {
                    gbr::Shared::Commands::AggressiveMoveTo::SetSpiritPos(GW::Maybe<GW::GamePos>::Nothing());
                    return;
                }
            }
            else {
                GW::Agents().Move(spiritPos.Value());
                return;
            }
        }


        auto tagetId = gbr::Shared::Commands::AggressiveMoveTo::GetTargetAgentId();
        if (tagetId) {
            auto agent = GW::Agents().GetAgentByID(tagetId);

            if (agent && agent->GetIsDead() && agent->IsPlayer()) {
                if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                    if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Rebirth, tagetId))
                        return;
                }
                else {
                    GW::Agents().Move(agent->pos);
                    return;
                }
            }
        }

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

        auto veilEffect = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Demonic_Miasma);
        bool isVeil = veilEffect.SkillId > 0;

        // if we can find the emo, see if we need to seed
        if (emo) {
            int lowHpCount = 0;
            bool tankIsLow = false;
            GW::Agent* target = nullptr;

            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
                auto agent = GW::Agents().GetAgentByID(id);

                if (isVeil && tank && agent->Id == tank->Id) {
                    if (agent->HP < 0.7f)
                        tankIsLow = true;
                }
                else if (agent && agent->HP < 0.5f) {
                    target = agent;
                    lowHpCount++;
                }
            }

            if (tankIsLow || lowHpCount > 2) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Seed_of_Life, emo->Id))
                    return;
            }
            else if (lowHpCount > 1 && player->Energy * player->MaxEnergy > 15) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Reversal_of_Fortune, target->Id))
                    return;
            }
        }

        if (tank && player->pos.SquaredDistanceTo(tank->pos) < GW::Constants::SqrRange::Spellcast) {
            bool hasBuff = false;
            auto buffs = GW::Effects().GetPlayerBuffArray();
            if (buffs.valid()) {
                for (auto buff : buffs) {
                    if (buff.SkillId == (DWORD)GW::Constants::SkillID::Life_Barrier && buff.TargetAgentId == tank->Id) {
                        hasBuff = true;
                        break;
                    }
                }
            }

            if (!hasBuff && SkillUtility::TryUseSkill(GW::Constants::SkillID::Life_Barrier, tank->Id))
                return;
        }
    }

    void BonderHandler::TickEmo() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

        sleepUntil = currentTime + 10;

        auto player = GW::Agents().GetPlayer();
        auto partyInfo = GW::Partymgr().GetPartyInfo();
        if (!player || player->GetIsDead() || !partyInfo || !partyInfo->players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
            return;

        auto players = partyInfo->players;
        if (!GW::Agents().GetPlayerArray().valid())
            return;

        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ether_Renewal, player->Id))
            return;

        auto pbond = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Protective_Bond);
        if (pbond.SkillId == 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, player->Id))
                return;
        }

        auto balth = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Balthazars_Spirit);
        if (balth.SkillId == 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Balthazars_Spirit, player->Id))
                return;
        }

        if (player->Energy < 0.5f || player->HP < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                return;

            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                return;
        }

        GW::Agent* tank = nullptr;
        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);
            if (agent && (agent->Primary == (BYTE)GW::Constants::Profession::Ranger || agent->Primary == (BYTE)GW::Constants::Profession::Assassin)) {
                tank = agent;
                break;
            }
        }

        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);

            if (id == player->Id)
                continue;

            auto agent = GW::Agents().GetAgentByID(id);

            if (!agent)
                continue;

            auto distance = agent->pos.SquaredDistanceTo(player->pos);

            if ((id == tank->Id && distance < GW::Constants::SqrRange::Earshot)
                || distance < GW::Constants::SqrRange::Spellcast) {

                if (agent->HP < 0.5f && SkillUtility::TryUseSkill(GW::Constants::SkillID::Infuse_Health, agent->Id))
                    return;

                if (agent->HP < 0.7f && SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, agent->Id))
                    return;
            }
        }

        // ensure the emo doesn't get stuck if he stops moving (since when not moving he spams spirit bond + burning speed)
        auto moveToPos = gbr::Shared::Commands::MoveTo::GetPos();
        if (moveToPos != GW::Maybe<GW::GamePos>::Nothing()) {
            return;
        }

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
                    if (!HasABond(id, lifeBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Life_Bond, id))
                                return;
                        }
                    }

                    if (!HasABond(id, balthBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Balthazars_Spirit, id))
                                return;
                        }
                    }
                }
                else if (!HasABond(id, protBonds)) {
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

        if (!player->GetIsMoving()) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                return;

            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                return;
        }
    }

    bool BonderHandler::HasABond(DWORD agentId, std::vector<GW::Buff> bonds) {
        for (auto bond : bonds) {
            if (bond.TargetAgentId == agentId)
                return true;
        }

        return false;
    }
}
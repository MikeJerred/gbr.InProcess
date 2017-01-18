#include <GWCA/GWCA.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Utilities/Maybe.h>
#include <gbr.Shared/Commands/AggressiveMoveTo.h>

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
        if (!player || player->GetIsDead())
            return;

        auto blessedAura = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Blessed_Aura);
        if (blessedAura.SkillId == 0) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Blessed_Aura, player->Id))
                return;
        }

        auto spiritPos = gbr::Shared::Commands::AggressiveMoveTo::GetSpiritPos();
        if (spiritPos != GW::Maybe<GW::GamePos>::Nothing()) {
            if (player->pos.SquaredDistanceTo(spiritPos.Value()) < GW::Constants::SqrRange::Adjacent) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Edge_of_Extinction, 0)) {
                    return;
                }

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

        auto players = GW::Partymgr().GetPartyInfo()->players;
        GW::Agent* emo = nullptr;
        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);

            if (agent
                && agent->Primary == (BYTE)GW::Constants::Profession::Elementalist
                && agent->Secondary == (BYTE)GW::Constants::Profession::Monk) {

                emo = agent;
            }
        }

        // if we can find the emo, see if we need to seed
        if (emo) {
            int lowHpCount = 0;

            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
                auto agent = GW::Agents().GetAgentByID(id);

                if (agent && agent->HP < 0.5f) {
                    lowHpCount++;
                }
            }

            if (lowHpCount > 2) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Seed_of_Life, emo->Id))
                    return;
            }
        }
    }

    void BonderHandler::TickEmo() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

        sleepUntil = currentTime + 10;

        auto player = GW::Agents().GetPlayer();
        auto players = GW::Partymgr().GetPartyInfo()->players;
        if (!player || player->GetIsDead() || !players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
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

        if (player->Energy < 0.5f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                return;

            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                return;
        }

        auto buffs = GW::Effects().GetPlayerBuffArray();
        if (buffs.valid()) {
            std::vector<GW::Buff> protBonds;

            for (auto buff : buffs) {
                if (buff.SkillId == (DWORD)GW::Constants::SkillID::Protective_Bond)
                    protBonds.push_back(buff);
            }

            auto agents = GW::Agents().GetAgentArray();
            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);

                if (id == player->Id)
                    continue;

                if (!HasABond(id, protBonds)) {
                    if (player->pos.SquaredDistanceTo(GW::Agents().GetAgentByID(id)->pos) < GW::Constants::SqrRange::Spellcast) {
                        if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Protective_Bond, id))
                            return;
                    }
                    else {
                        GW::Agents().Move(GW::Agents().GetAgentByID(id)->pos);
                        return;
                    }
                }
            }
        }

        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);

            if (agent && agent->HP < 0.5f) {
                if (agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {

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
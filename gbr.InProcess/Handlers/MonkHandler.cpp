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
#include "MonkHandler.h"

namespace gbr::InProcess {
    using LogUtility = Utilities::LogUtility;
    using SkillUtility = Utilities::SkillUtility;

    MonkHandler::MonkHandler(Utilities::PlayerType playerType) : playerType(playerType) {
        hookId = GW::Gamethread().AddPermanentCall([this]() {
            Tick();
        });
    }

    MonkHandler::~MonkHandler() {
        GW::Gamethread().RemovePermanentCall(hookId);
    }

    void MonkHandler::Tick() {
        static long long sleepUntil = 0;

        long long currentTime = GetTickCount();
        if ((sleepUntil - currentTime) > 0)
            return;

        sleepUntil = currentTime + 10;

        if (!GW::Map().IsMapLoaded()) {
            gbr::Shared::Commands::MoveTo::ClearPos();
            sleepUntil += 1000;
            LogUtility::Log(L"Waiting for map load...");
            return;
        }

        if (GW::Map().GetInstanceType() != GW::Constants::InstanceType::Explorable)
            return;

        auto player = GW::Agents().GetPlayer();
        auto agents = GW::Agents().GetAgentArray();
        auto partyInfo = GW::Partymgr().GetPartyInfo();
        if (!player || player->GetIsDead() || !agents.valid() || !partyInfo || !partyInfo->players.valid() || GW::Skillbar::GetPlayerSkillbar().Casting)
            return;

        LogUtility::Log(L"Starting valid cycle");

        auto players = partyInfo->players;

        auto veilEffect = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Demonic_Miasma);
        bool isVeil = veilEffect.SkillId > 0;

        auto foundryEffect = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Enduring_Torment);
        bool isFoundry = foundryEffect.SkillId > 0;

        static long long waitUntilToPing = 0;
        if (isFoundry && (waitUntilToPing - currentTime) <= 0) {
            for (auto agent : agents) {
                if (agent
                    && !agent->GetIsDead()
                    && !agent->IsPlayer()
                    && agent->Allegiance == 3
                    && agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Compass) {

                    LogUtility::Log(L"Ping for snakes");
                    GW::Agents().CallTarget(agent);
                    waitUntilToPing = currentTime + 10000;
                    return;
                }
            }
        }

        LogUtility::Log(L"We don't need to ping for snakes!");

        auto blessedAura = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Blessed_Aura);
        if (blessedAura.SkillId == 0) {
            LogUtility::Log(L"Use Blessed Aura");
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Blessed_Aura, player->Id))
                return;
        }

        LogUtility::Log(L"We have blessed aura!");

        auto spiritPos = gbr::Shared::Commands::PlaceSpirit::GetSpiritPos();
        if (spiritPos.HasValue()) {
            if (player->pos.SquaredDistanceTo(spiritPos.Value()) < GW::Constants::SqrRange::Adjacent) {
                LogUtility::Log(L"Use EoE");
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Edge_of_Extinction, 0))
                    return;

                /* LogUtility::Log(L"Use Winnowing");
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Winnowing, 0)) {
                return;
                }*/

                gbr::Shared::Commands::PlaceSpirit::ClearSpiritPos();
            }
            else {
                LogUtility::Log(L"Moving to spirit pos...");
                GW::Agents().Move(spiritPos.Value());
                return;
            }
        }

        LogUtility::Log(L"We don't need to cast spirits!");

        LogUtility::Log(L"See if we need to rez anyone!");
        for (auto agent : agents) {
            if (agent && agent->GetIsDead() && agent->IsPlayer() && agent->Id != player->Id && agent->pos.SquaredDistanceTo(player->pos) < GW::Constants::SqrRange::Spellcast) {
                auto buffs = GW::Effects().GetPlayerBuffArray();
                if (buffs.valid()) {
                    for (auto buff : buffs) {
                        if (buff.SkillId == (DWORD)GW::Constants::SkillID::Unyielding_Aura) {
                            sleepUntil += 500;
                            GW::Effects().DropBuff(buff.BuffId);
                            return;
                        }
                    }
                }
            }
        }

        LogUtility::Log(L"We don't need to rez anyone!");

        LogUtility::Log(L"Finding tank & emo...");
        GW::Agent* emo = nullptr;
        GW::Agent* tank = nullptr;
        for (auto p : players) {
            auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
            auto agent = GW::Agents().GetAgentByID(id);

            if (agent && !agent->GetIsDead()) {
                if (agent->Primary == (BYTE)GW::Constants::Profession::Elementalist && agent->Secondary == (BYTE)GW::Constants::Profession::Monk) {
                    emo = agent;
                }
                else if (agent->Primary == (BYTE)GW::Constants::Profession::Ranger || agent->Primary == (BYTE)GW::Constants::Profession::Assassin) {
                    tank = agent;
                }
            }
        }

        LogUtility::Log(L"Found tank and emo!");

        // if we can find the emo, see if we need to seed
        if (emo) {
            LogUtility::Log(L"emo found!");

            int lowHpCount = 0;
            bool tankIsLow = false;
            bool emoIsLow = false;
            GW::Agent* target = nullptr;

            for (auto p : players) {
                auto id = GW::Agents().GetAgentIdByLoginNumber(p.loginnumber);
                auto agent = GW::Agents().GetAgentByID(id);

                if (!agent || agent->GetIsDead())
                    continue;

                if (isVeil && tank && agent->Id == tank->Id) {
                    if (agent->HP < 0.6f)
                        tankIsLow = true;
                }
                else if (agent->Id == emo->Id) {
                    if (agent->HP < 0.4f)
                        emoIsLow = true;
                }
                else if (agent->HP < 0.5f && (!tank || tank->Id != agent->Id)) {
                    target = agent;
                    lowHpCount++;
                }
            }

            LogUtility::Log(L"See if we need to heal anyone...");

            if (tankIsLow || emoIsLow || lowHpCount > 2) {
                LogUtility::Log(L"Using seed");
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Seed_of_Life, emo->Id))
                    return;
            }
            /*else if (lowHpCount > 1 && player->Energy * player->MaxEnergy > 15) {
            LogUtility::Log(L"Healing with ebon escape");
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ebon_Escape, target->Id))
            return;
            }*/
        }

        LogUtility::Log(L"See if we need to cast bonds...");
        auto buffs = GW::Effects().GetPlayerBuffArray();
        if (buffs.valid()) {
            std::vector<GW::Buff> lifeBonds;
            auto hasUa = false;

            for (auto buff : buffs) {
                switch (buff.SkillId) {
                case (DWORD)GW::Constants::SkillID::Life_Bond:
                    lifeBonds.push_back(buff);
                    break;
                case (DWORD)GW::Constants::SkillID::Unyielding_Aura:
                    hasUa = true;
                    break;
                }
            }

            if (!hasUa && SkillUtility::TryUseSkill(GW::Constants::SkillID::Unyielding_Aura, player->Id))
                return;

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
                }
                else if (emo && emo->Id == id) {
                    LogUtility::Log(L"Ensure emo is bonded");
                    if (!HasABond(id, lifeBonds)) {
                        if (player->pos.SquaredDistanceTo(agent->pos) < GW::Constants::SqrRange::Spellcast) {
                            LogUtility::Log(L"Using life bond on  emo");
                            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Life_Bond, id))
                                return;
                        }
                    }
                }
            }
        }

        LogUtility::Log(L"Nothing to do");
    }

    bool MonkHandler::HasABond(DWORD agentId, std::vector<GW::Buff> bonds) {
        for (auto bond : bonds) {
            if (bond.TargetAgentId == agentId)
                return true;
        }

        return false;
    }
}
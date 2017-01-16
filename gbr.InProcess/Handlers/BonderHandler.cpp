#include <GWCA/GWCA.h>
#include <GWCA/Managers/EffectMgr.h>
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

    bool BonderHandler::ShouldSleep(long long& sleepUntil) {
        long long currentTime = GetTickCount();
        return (sleepUntil - currentTime) > 0;
    }

    void BonderHandler::TickMonk() {
        static long long sleepUntil = 0;
        if (ShouldSleep(sleepUntil))
            return;

        sleepUntil = GetTickCount() + 10;

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
            }
        }
    }

    void BonderHandler::TickEmo() {
        static long long sleepUntil = 0;
        if (ShouldSleep(sleepUntil))
            return;

        sleepUntil = GetTickCount() + 10;

        auto player = GW::Agents().GetPlayer();
        if (!player || player->GetIsDead())
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

        if (player->Energy < 0.75f) {
            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Spirit_Bond, player->Id))
                return;

            if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Burning_Speed, player->Id))
                return;
        }

        auto buffs = GW::Effects().GetPlayerBuffArray();

        if (buffs.valid()) {
            for (auto buff : buffs) {
                buff.
            }
        }
    }
}
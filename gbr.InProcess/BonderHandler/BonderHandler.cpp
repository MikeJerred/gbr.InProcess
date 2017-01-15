#include <GWCA/GWCA.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Utilities/Maybe.h>
#include <gbr.Shared/Commands/AggressiveMoveTo.h>

#include "../Utilities/SkillUtility.h"
#include "BonderHandler.h"

namespace gbr::InProcess {
    using SkillUtility = Utilities::SkillUtility;

    BonderHandler* BonderHandler::instance = nullptr;

    DWORD WINAPI BonderHandler::ThreadEntry(LPVOID) {
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;

        auto isEmo = false;

        switch (skills[0].SkillId) {
        default:
            return TRUE;
        case (DWORD)GW::Constants::SkillID::Ether_Renewal:
            isEmo = true;
            break;
        case (DWORD)GW::Constants::SkillID::Life_Barrier:
            isEmo = false;
            break;
        }

        instance = new BonderHandler(isEmo);

        return TRUE;
    }

    BonderHandler::BonderHandler(bool isEmo) : isEmo(isEmo) {
        static long long sleepUntil = 0;

        hookGuid = GW::Gamethread().AddPermanentCall([=]() {
            long long currentTime = GetTickCount();

            if ((sleepUntil - currentTime) > 0) {
                return;
            }

            sleepUntil = currentTime + 10;

            auto player = GW::Agents().GetPlayer();
            auto agents = GW::Agents().GetAgentArray();

            if (!player || player->GetIsDead() || !agents.valid())
                return;

            if (isEmo) {
                if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Ether_Renewal, 0))
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
            }
            else {
                auto blessedAura = GW::Effects().GetPlayerEffectById(GW::Constants::SkillID::Blessed_Aura);
                if (blessedAura.SkillId == 0) {
                    if (SkillUtility::TryUseSkill(GW::Constants::SkillID::Blessed_Aura, player->Id))
                        return;
                }
            }

            auto spiritPos = gbr::Shared::Commands::AggressiveMoveTo::GetSpiritPos();
            if (!isEmo && spiritPos != GW::Maybe<GW::GamePos>::Nothing()) {
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


        });
    }


    BonderHandler::~BonderHandler() {
        GW::Gamethread().RemovePermanentCall(hookGuid);
    }
}
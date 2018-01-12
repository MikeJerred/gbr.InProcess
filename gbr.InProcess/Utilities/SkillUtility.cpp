#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "SkillUtility.h"

namespace gbr::InProcess::Utilities {
    bool SkillUtility::TryUseSkill(GW::Constants::SkillID skillId, DWORD targetId) {
        auto player = GW::Agents::GetPlayer();
        auto energy = player->Energy * player->MaxEnergy;

        if (energy >= GW::SkillbarMgr::GetSkillConstantData((DWORD)skillId).GetEnergyCost()) {
            auto skill = GW::Skillbar::GetPlayerSkillbar().GetSkillById(skillId);
            if (skill.HasValue() && skill.Value().GetRecharge() == 0 && !GW::Skillbar::GetPlayerSkillbar().Casting) {
                GW::SkillbarMgr::UseSkill(GW::SkillbarMgr::GetSkillSlot(skillId), targetId);
                return true;
            }
        }

        return false;
    }
}
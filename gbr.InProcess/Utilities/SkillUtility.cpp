#include "SkillUtility.h"

namespace gbr::InProcess::Utilities {
    bool SkillUtility::TryUseSkill(GW::Constants::SkillID skillId, DWORD targetId) {
        auto skill = GW::Skillbar::GetPlayerSkillbar().GetSkillById(skillId);
        if (skill.SkillId > 0 && skill.GetRecharge() == 0 && !GW::Skillbar::GetPlayerSkillbar().Casting) {
            GW::Skillbarmgr().UseSkill(GW::Skillbarmgr().GetSkillSlot(skillId), targetId);
            return true;
        }

        return false;
    }
}
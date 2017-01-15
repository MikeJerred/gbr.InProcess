#pragma once

#include <GWCA/GWCA.h>
#include <GWCA/Managers/SkillbarMgr.h>

namespace gbr::InProcess::Utilities {
    class SkillUtility {
    public:
        static bool TryUseSkill(GW::Constants::SkillID skillId, DWORD targetId);
    };
}
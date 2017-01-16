#include <GWCA/GWCA.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "PlayerUtility.h"

namespace gbr::InProcess::Utilities {
    PlayerType PlayerUtility::GetPlayerType() {
        auto skills = GW::Skillbar::GetPlayerSkillbar().Skills;

        for (int i = 0; i < 8; ++i) {
            auto skill = skills[i];

            switch (skill.SkillId) {
            default: continue;
            case (DWORD)GW::Constants::SkillID::Visions_of_Regret: return PlayerType::VoR;
            case (DWORD)GW::Constants::SkillID::Energy_Surge:
            {
                auto loginNumber = GW::Agents().GetPlayer()->LoginNumber;
                auto players = GW::Partymgr().GetPartyInfo()->players;

                for (int i = 0; i < players.size(); ++i) {
                    auto player = players[i];
                    if (player.loginnumber == loginNumber) {
                        switch (i % 4) {
                        case 0: return PlayerType::ESurge1;
                        case 1: return PlayerType::ESurge2;
                        case 2: return PlayerType::ESurge3;
                        case 3: return PlayerType::ESurge4;
                        }
                    }
                }

                return PlayerType::Unknown;
            }
            case (DWORD)GW::Constants::SkillID::Life_Barrier: return PlayerType::Monk;
            case (DWORD)GW::Constants::SkillID::Ether_Renewal: return PlayerType::Emo;
            }
        }

        return PlayerType::Unknown;
    }
}
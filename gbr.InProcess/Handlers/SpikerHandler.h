#pragma once

#include <Windows.h>

#include "../Utilities/PlayerUtility.h"

namespace gbr::InProcess {
    class SpikerHandler {
    private:
        Utilities::PlayerType playerType;
        GUID hookGuid;

        void Tick();

        void SpikeTarget(GW::Agent* target);
        void SpikeAsVoR(GW::Agent* target, GW::Agent* overloadTarget);
        void SpikeAsESurge(GW::Agent* target, GW::Agent* overloadTarget);

        static DWORD GetQuestTakeDialog(DWORD quest) { return (quest << 8) | 0x800001; };
        static DWORD GetQuestUpdateDialog(DWORD quest) { return (quest << 8) | 0x800004; };
        static DWORD GetQuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; };
        static void JumpToTarget(GW::Agent* target);
        static void AcceptNextDialog();
        static std::vector<GW::Agent*> GetOtherEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort = nullptr);
        static std::vector<GW::Agent*> GetEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort = nullptr);
        static bool HasHighEnergy(GW::Agent* agent);
        static bool HasHighAttackRate(GW::Agent* agent);
        static std::wstring GetAgentName(GW::Agent* agent);
    public:
        SpikerHandler(Utilities::PlayerType playerType);
        ~SpikerHandler();
    };
}
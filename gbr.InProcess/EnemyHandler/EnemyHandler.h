#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    enum class PlayerType {
        Unknown,
        Tank,
        Vor,
        ESurge,
        Bonder,
        Rez
    };

    class EnemyHandler {
    public:
        static DWORD WINAPI ThreadEntry(LPVOID);

        EnemyHandler(PlayerType playerType) : playerType(playerType) {};
        void Listen();
        void SpikeTarget(GW::Agent* target);
        void JumpToTarget(GW::Agent* target);
        bool RessurectTarget(GW::Agent* target);
        void SendDialog(GW::Agent* npc);
    private:
        DWORD GetQuestTakeDialog(DWORD quest) { return (quest << 8) | 0x800001; };
        DWORD GetQuestUpdateDialog(DWORD quest) { return (quest << 8) | 0x800004; };
        DWORD GetQuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; };

        void SpikeAsVoR(GW::Agent* target);
        void SpikeAsESurge(GW::Agent* target, unsigned int n);
        void SpikeAsRez(GW::Agent* target);
        std::vector<GW::Agent*> GetOtherEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort = nullptr);
        bool HasHighEnergy(GW::Agent* agent);
        bool HasHighAttackRate(GW::Agent* agent);
        std::wstring ModelIdToString(int id);

        PlayerType playerType;
    };
}
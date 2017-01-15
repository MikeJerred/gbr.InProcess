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
        static EnemyHandler* instance;
        static DWORD WINAPI ThreadEntry(LPVOID);

        EnemyHandler(PlayerType playerType, int esurgeNumber);
        ~EnemyHandler();
        void SpikeTarget(GW::Agent* target);
        static void JumpToTarget(GW::Agent* target);
        static bool RessurectTarget(GW::Agent* target);
        static void AcceptNextDialog();

    private:
        GUID hookGuid;

        static DWORD GetQuestTakeDialog(DWORD quest) { return (quest << 8) | 0x800001; };
        static DWORD GetQuestUpdateDialog(DWORD quest) { return (quest << 8) | 0x800004; };
        static DWORD GetQuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; };

        void SpikeAsVoR(GW::Agent* target, GW::Agent* overloadTarget);
        void SpikeAsESurge(GW::Agent* target, unsigned int n, GW::Agent* overloadTarget);
        void SpikeAsRez(GW::Agent* target);
        static std::vector<GW::Agent*> GetOtherEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort = nullptr);
        static std::vector<GW::Agent*> GetEnemiesInRange(GW::Agent* target, float range, std::function<bool(GW::Agent*, GW::Agent*)> sort = nullptr);
        static bool HasHighEnergy(GW::Agent* agent);
        static bool HasHighAttackRate(GW::Agent* agent);

        PlayerType playerType;
        int esurgeNumber;
    };
}
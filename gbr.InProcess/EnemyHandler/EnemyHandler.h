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
        void SpikeTarget(GW::Agent* enemy);
        void SendDialog(GW::Agent* npc);
    private:
        DWORD GetQuestTakeDialog(DWORD quest) { return (quest << 8) | 0x800001; };
        DWORD GetQuestUpdateDialog(DWORD quest) { return (quest << 8) | 0x800004; };
        DWORD GetQuestRewardDialog(DWORD quest) { return (quest << 8) | 0x800007; };

        PlayerType playerType;
    };
}
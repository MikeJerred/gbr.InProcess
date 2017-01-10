#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    enum class SpikerType {
        None,
        ESurge,
        Vor
    };

    class EnemyHandler {
    public:
        static DWORD WINAPI ThreadEntry(LPVOID);

        EnemyHandler(SpikerType spikerType) : spikerType(spikerType) {};
        void Listen();
        void SpikeTarget(GW::Agent* enemy);
    private:
        SpikerType spikerType;
    };
}
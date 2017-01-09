#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    class DropsHandler {
    public:
        static DWORD WINAPI ThreadEntry(LPVOID);

        void Listen();
        static void PickupNearbyGem();
    private:
        static const std::initializer_list<int> wantedModelIds;
    };
}
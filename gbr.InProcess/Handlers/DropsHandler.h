#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    class DropsHandler {
    private:
        static const std::initializer_list<int> wantedModelIds;
        DWORD hookId;

        static void Tick();
        static bool PickupNearbyGem();
    public:
        DropsHandler();
        ~DropsHandler();
    };
}
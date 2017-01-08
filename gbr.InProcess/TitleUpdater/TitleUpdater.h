#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    class TitleUpdater {
    public:
        static DWORD WINAPI ThreadEntry(LPVOID);
    };
}
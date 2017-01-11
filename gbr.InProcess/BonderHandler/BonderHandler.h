#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    class BonderHandler {
    public:
        static DWORD WINAPI ThreadEntry(LPVOID);
    };
}
#pragma once

#include <Windows.h>

namespace gbr::InProcess {
    class BonderHandler {
    public:
        static BonderHandler* instance;

        static DWORD WINAPI ThreadEntry(LPVOID);

        BonderHandler(bool isEmo);
        ~BonderHandler();
    private:
        GUID hookGuid;
        bool isEmo;
    };
}
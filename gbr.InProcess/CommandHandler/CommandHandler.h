#pragma once

#include <Windows.h>

#include "Commands/Commands.h"

namespace gbr::InProcess {
    class CommandHandler {
    private:
        std::wstring pipeName;
        HANDLE pipeHandle;
        static const DWORD bufferSize = 0x1000;

        CommandHandler(std::wstring pipeName) : pipeName(pipeName), pipeHandle(INVALID_HANDLE_VALUE) {};
        void Connect();
        void Disconnect();
        void Listen();
    public:
        static DWORD WINAPI ThreadEntry(HMODULE hModule);

        template<class TCommandRequest>
        void Send(TCommandRequest request) {
            DWORD bytesWritten;
            WriteFile(pipeHandle, &request, sizeof(TCommandRequest), &bytesWritten, NULL);
        }
    };
}
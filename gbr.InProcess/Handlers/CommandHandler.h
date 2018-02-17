#pragma once

#include <thread>
#include <vector>
#include <Windows.h>

namespace gbr::InProcess {
    class CommandHandler {
    private:
        static constexpr DWORD bufferSize = 0x1000;
        HMODULE hModule;
        std::wstring pipeName;
        std::thread connectionThread;
        std::vector<std::thread> listenThreads;
		bool mustQuit;

        void Connect();
        void Listen(HANDLE pipeHandle);
    public:
        CommandHandler(HMODULE hModule, std::wstring playerName);
		~CommandHandler();

        /*template<class TCommandRequest>
        void Send(TCommandRequest request) {
            DWORD bytesWritten;
            WriteFile(pipeHandle, &request, sizeof(TCommandRequest), &bytesWritten, NULL);
        }*/
    };
}
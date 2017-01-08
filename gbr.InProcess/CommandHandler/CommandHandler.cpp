#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/PlayerMgr.h>

#include "CommandHandler.h"
#include "Commands/Commands.h"

namespace gbr::InProcess {
    DWORD WINAPI CommandHandler::ThreadEntry(HMODULE hModule) {
        auto charName = GW::Playermgr().GetPlayerName(0);
        auto pipeName = std::wstring(L"\\\\.\\pipe\\gbr_") + charName;
        auto instance = CommandHandler(pipeName);

        instance.Connect();

        try {
            instance.Listen();
        }
        catch (...) {
            instance.Disconnect();
        }

        FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
    }

    void CommandHandler::Connect() {
        pipeHandle = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            CommandHandler::bufferSize,
            CommandHandler::bufferSize,
            0,
            NULL);

        if (pipeHandle == INVALID_HANDLE_VALUE) {
            throw std::exception("Failed to create pipe. Error code: %d", GetLastError());
        }

        auto connected = ConnectNamedPipe(pipeHandle, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;

        if (connected) {

        }
        else {
            CloseHandle(pipeHandle);
        }
    }

    void CommandHandler::Disconnect() {
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(pipeHandle);
            CloseHandle(pipeHandle);
        }
    }

    void CommandHandler::Listen() {
        const auto bufferSize = 0x1000;
        BYTE buffer[bufferSize];
        DWORD bytesRead;

        while (pipeHandle != INVALID_HANDLE_VALUE) {
            if (ReadFile(pipeHandle, buffer, bufferSize, &bytesRead, NULL)) {
                Command::ExecuteCommand((Command::BaseRequest*)buffer);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
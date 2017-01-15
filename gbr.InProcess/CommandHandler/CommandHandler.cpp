#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/AgentMgr.h>
#include <gbr.Shared/Commands/BaseCommand.h>

#include "CommandHandler.h"

namespace gbr::InProcess {
    using BaseCommand = gbr::Shared::Commands::BaseCommand;

    DWORD WINAPI CommandHandler::ThreadEntry(HMODULE hModule) {
        auto loginNumber = GW::Agents().GetPlayer()->LoginNumber;
        auto charName = GW::Agents().GetPlayerNameByLoginNumber(loginNumber);
        auto pipeName = std::wstring(L"\\\\.\\pipe\\gbr_") + charName;
        auto instance = CommandHandler(pipeName);

        instance.Connect();

        auto listenThread = std::thread([&]() { instance.Listen(); });

        while (instance.pipeHandle != INVALID_HANDLE_VALUE) {
            instance.Connect();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        listenThread.join();

        FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
    }

    CommandHandler::CommandHandler(std::wstring pipeName) : pipeName(pipeName) {
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
    }

    CommandHandler::~CommandHandler() {
        Disconnect();
    }

    bool CommandHandler::Connect() {
        return ConnectNamedPipe(pipeHandle, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
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
                if (BaseCommand::ExecuteCommand((BaseCommand::BaseRequest*)buffer)) {
                    Disconnect();
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
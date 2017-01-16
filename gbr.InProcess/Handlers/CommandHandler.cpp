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

    CommandHandler::CommandHandler(HMODULE hModule, std::wstring playerName) : hModule(hModule) {
        pipeName = std::wstring(L"\\\\.\\pipe\\gbr_") + playerName;

        connectionThread = std::thread([this]() {
            Connect();
        });
    }

    void CommandHandler::Connect() {
        while (true) {
            auto pipeHandle = CreateNamedPipeW(
                pipeName.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                CommandHandler::bufferSize,
                CommandHandler::bufferSize,
                0,
                NULL);

            if (pipeHandle == INVALID_HANDLE_VALUE) {
                throw std::exception("Failed to create pipe. Error code: %d.\n", GetLastError());
            }

            auto connected = ConnectNamedPipe(pipeHandle, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;

            if (connected) {
                auto listenThread = std::thread(
                    [this](HANDLE pipeHandle) {
                        Listen(pipeHandle);
                    },
                    pipeHandle);

                listenThreads.push_back(listenThread);
            }
            else {
                CloseHandle(pipeHandle);
            }
        }
    }

    void CommandHandler::Listen(HANDLE pipeHandle) {
        BYTE buffer[bufferSize];
        DWORD bytesRead;

        while (pipeHandle != INVALID_HANDLE_VALUE) {
            if (ReadFile(pipeHandle, buffer, bufferSize, &bytesRead, NULL)) {
                if (BaseCommand::ExecuteCommand((BaseCommand::BaseRequest*)buffer)) {
                    FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
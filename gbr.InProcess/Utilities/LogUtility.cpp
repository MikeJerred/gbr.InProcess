#include <stdio.h>
#include <time.h>

#include "LogUtility.h"

namespace gbr::InProcess::Utilities {
    FILE* LogUtility::logFile = nullptr;

    void LogUtility::Init(std::wstring charName) {
        auto fileName = L"log - " + charName + L".txt";

        _wfopen_s(&logFile, fileName.c_str(), L"w");
    }

    void LogUtility::Close() {
        if (logFile)
            fclose(logFile);
    }

    void LogUtility::Log(std::wstring str) {
        if (!logFile)
            return;

        time_t rawtime;
        time(&rawtime);

        struct tm timeinfo;
        localtime_s(&timeinfo, &rawtime);

        wchar_t buffer[16];
        wcsftime(buffer, sizeof(buffer), L"%H:%M:%S", &timeinfo);

        fwprintf(logFile, L"[%s] %s", buffer, (str + L"\n").c_str());

        fflush(logFile);
    }
}
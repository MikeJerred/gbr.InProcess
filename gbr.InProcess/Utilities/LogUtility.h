#pragma once

#include <string>

namespace gbr::InProcess::Utilities {

    class LogUtility {
    private:
        static FILE* logFile;
    public:
        static void Init(std::wstring charName);
        static void Close();
        static void Log(std::wstring str);
    };
}
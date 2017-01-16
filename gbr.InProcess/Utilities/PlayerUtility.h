#pragma once

namespace gbr::InProcess::Utilities {
    enum class PlayerType {
        VoR,
        ESurge1,
        ESurge2,
        ESurge3,
        ESurge4,
        Monk,
        Emo,
        Unknown
    };

    class PlayerUtility {
    public:
        static PlayerType GetPlayerType();
    };
}
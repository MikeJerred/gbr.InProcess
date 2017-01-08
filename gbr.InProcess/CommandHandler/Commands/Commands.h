#pragma once

#include <Windows.h>
#include <GWCA/GWCA.h>

namespace gbr::InProcess {
    enum class CommandType {
        MoveTo,
        UseSkill,
        DropBuff
    };


    class Command {
    public:
        struct BaseRequest {
        protected:
            BaseRequest(CommandType type) : type(type) {}
        public:
            const CommandType type;
        };

        struct BaseReply {
        protected:
            BaseReply(CommandType type) : type(type) {}
        public:
            const CommandType type;
        };

        static void ExecuteCommand(BaseRequest* request);
    };


    class MoveTo : public Command {
    public:
        struct Request : public BaseRequest {
            Request() : BaseRequest(CommandType::MoveTo) {}
            Request(const char* s);

            float x;
            float y;
            DWORD zPlane;
        };

        static void Execute(Request* request);
    };

    class UseSkill : public Command {
    public:
        struct Request : public BaseRequest {
            Request() : BaseRequest(CommandType::UseSkill) {}
            Request(const char* s);

            GW::Constants::SkillID skillId;
            DWORD targetId;
            bool ping;
        };

        static void Execute(Request* request);
    };

    class DropBuff : public Command {
    public:
        struct Request : public BaseRequest {
            Request() : BaseRequest(CommandType::DropBuff) {}
            Request(const char* s);

            GW::Constants::SkillID skillId;
        };

        static void Execute(Request* request);
    };
}
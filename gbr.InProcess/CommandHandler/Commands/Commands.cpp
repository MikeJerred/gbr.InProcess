#include <exception>
#include <json/json.hpp>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "Commands.h"

namespace gbr::InProcess {
    using json = nlohmann::json;

    void Command::ExecuteCommand(BaseRequest* request) {
        switch (request->type) {
        case CommandType::MoveTo:
            MoveTo::Execute(static_cast<MoveTo::Request*>(request));
            break;
        case CommandType::UseSkill:
            UseSkill::Execute(static_cast<UseSkill::Request*>(request));
            break;
        case CommandType::DropBuff:
            DropBuff::Execute(static_cast<DropBuff::Request*>(request));
            break;
        default:
            throw std::invalid_argument(std::string("Invalid Command Type: ") + std::to_string((int)request->type));
        }
    }

    #pragma region MoveTo

    MoveTo::Request::Request(const char* s) : Request() {
        auto obj = json::parse(s);
        auto x = obj["x"];
        auto y = obj["y"];
        auto zPlane = obj["zPlane"];

        if (!x.is_number() || !y.is_number() || (!zPlane.is_number_integer() && !zPlane.is_null())) {
            throw new std::invalid_argument(std::string("Invalid MoveTo command: { ")
                + "x: " + x.get<std::string>()
                + ", y: " + y.get<std::string>()
                + ", zPlane: " + zPlane.get<std::string>()
                + " }");
        }

        this->x = x.get<float>();
        this->y = y.get<float>();
        this->zPlane = zPlane.is_null() ? 0 : zPlane.get<int>();
    }

    void MoveTo::Execute(Request* request) {
        GW::Agents().Move(request->x, request->y, request->zPlane);
    }

    #pragma endregion

    #pragma region UseSkill

    UseSkill::Request::Request(const char* s) : Request() {
        auto obj = json::parse(s);
        auto skillId = obj["skillId"];
        auto targetId = obj["targetId"];
        auto ping = obj["ping"];

        if (!skillId.is_number_unsigned()
            || (!targetId.is_number_unsigned() && !targetId.is_null())
            || !ping.is_boolean()) {

            throw new std::invalid_argument(std::string("Invalid UseSkill command: { ")
                + "skillId: " + skillId.get<std::string>()
                + ", targetId: " + targetId.get<std::string>()
                + ", ping: " + ping.get<std::string>()
                + " }");
        }

        this->skillId = static_cast<GW::Constants::SkillID>(skillId.get<DWORD>());
        this->targetId = targetId.get<DWORD>();
        this->ping = ping.get<bool>();
    }

    void UseSkill::Execute(Request* request) {
        GW::Gamethread().Enqueue([=]() {
            GW::Skillbarmgr().UseSkillByID(static_cast<DWORD>(request->skillId), request->targetId, request->ping);
        });
    }

    #pragma endregion

    #pragma region DropBuff

    DropBuff::Request::Request(const char* s) : Request() {
        auto obj = json::parse(s);
        auto skillId = obj["skillId"];

        if (!skillId.is_number_unsigned()) {
            throw new std::invalid_argument(std::string("Invalid DropBuff command: { ")
                + "skillId: " + skillId.get<std::string>()
                + " }");
        }

        this->skillId = static_cast<GW::Constants::SkillID>(skillId.get<DWORD>());
    }

    void DropBuff::Execute(Request* request) {
        GW::Gamethread().Enqueue([=]() {
            auto buff = GW::Effects().GetPlayerBuffBySkillId(request->skillId);
            if (buff.BuffId != 0)
                GW::Effects().DropBuff(buff.BuffId);
        });
    }

    #pragma endregion
}
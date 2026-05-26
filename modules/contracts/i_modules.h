#pragma once

#include <cstdint>
#include <string>

#include "cmds/cmd_dtos.h"
#include "cmds/cmd_parse_results.h"
#include "cmds/cmd_exec_results.h"
#include "err.h"

namespace kvdb::contracts {
    struct AccessInterfaceStartOptions final
    {
        // 0 means: choose any currently free TCP port automatically.
        std::uint16_t preferredPort = 0;
    };

    class IEngine
    {
    public:
        virtual ~IEngine() = default;

        [[nodiscard]]
        virtual CmdExecResult execute(const BaseCmdDto& cmd) = 0;

        virtual void onInstanceStart() = 0;

        virtual void onInstanceShutdown() = 0;
    };

    class IQueryParser
    {
    public:
        virtual ~IQueryParser() = default;

        [[nodiscard]]
        virtual CmdParseResult parse(const std::string& rawQuery) = 0;
    };

    class IResponseConstructor
    {
    public:
        virtual ~IResponseConstructor() = default;

        [[nodiscard]]
        virtual std::string buildSuccessResponse(
            const SuccessCmdExecResult& success
        ) = 0;

        [[nodiscard]]
        virtual std::string buildErrResponse(
            const Err& err
        ) = 0;

        [[nodiscard]]
        virtual std::string buildSessionStartedResponse() = 0;

        [[nodiscard]]
        virtual std::string buildSessionEndedResponse() = 0;
    };

    class IAccessInterface
    {
    public:
        virtual ~IAccessInterface() = default;

        virtual void start(
            IQueryParser& queryParser,
            IEngine& engine,
            IResponseConstructor& responseConstructor,
            const AccessInterfaceStartOptions& startOptions
        ) = 0;

        [[nodiscard]]
        virtual std::uint16_t boundPort() const noexcept = 0;

        virtual void requestStop() = 0;
    };
}

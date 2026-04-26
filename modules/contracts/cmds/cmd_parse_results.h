#pragma once

#include <memory>
#include <variant>

#include "cmd_dtos.h"
#include "../err.h"

namespace kvdb::contracts {
    using CmdParseSuccess = std::unique_ptr<BaseCmdDto>;

    // Err is abstract, so parse errors must be passed by pointer.
    using CmdParseErr = std::shared_ptr<const Err>;

    using CmdParseResult = std::variant<
        CmdParseSuccess,
        CmdParseErr
    >;
}
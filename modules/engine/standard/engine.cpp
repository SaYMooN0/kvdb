#include "i_modules.h"

#include <variant>

namespace kvdb::modules::engine::standard {
    class StandardEngine final : public kvdb::contracts::IEngine
    {
    public:
        kvdb::contracts::CmdExecResult execute(
            const kvdb::contracts::BaseCmdDto& cmd
        ) override {
            using namespace kvdb::contracts;

            switch (cmd.kind()) {
            case CmdKind::Begin:
                return SuccessCmdExecResult{
                    TransactionOpCmdExecSuccess{
                        .operation = TransactionOpKind::Begin
                    }
                };

            case CmdKind::Commit:
                return SuccessCmdExecResult{
                    TransactionOpCmdExecSuccess{
                        .operation = TransactionOpKind::Commit
                    }
                };

            case CmdKind::Rollback:
                return SuccessCmdExecResult{
                    TransactionOpCmdExecSuccess{
                        .operation = TransactionOpKind::Rollback
                    }
                };

            case CmdKind::AnyTransaction:
                return SuccessCmdExecResult{
                    TransactionCheckCmdExecSuccess{
                        .transactionActive = false
                    }
                };

            case CmdKind::Set:
            case CmdKind::Del:
            case CmdKind::EnsureDel:
                return SuccessCmdExecResult{
                    AffectedRowsCmdExecSuccess{
                        .count = 1
                    }
                };

            case CmdKind::Get:
                return SuccessCmdExecResult{
                    GetCmdExecSuccess{
                        .value = ColCmdValue{}
                    }
                };

            case CmdKind::CreateTable:
            case CmdKind::EraseTable:
            case CmdKind::EnsureTableErased:
                return SuccessCmdExecResult{
                    EmptyCmdExecSuccess{}
                };

            case CmdKind::TableInfo:
                return SuccessCmdExecResult{
                    EmptyCmdExecSuccess{}
                };
            }

            return SuccessCmdExecResult{
                EmptyCmdExecSuccess{}
            };
        }

        void onInstanceStart() override {}

        void onInstanceShutdown() override {}
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IEngine* create_engine() {
    return new kvdb::modules::engine::standard::StandardEngine();
}

extern "C" __declspec(dllexport)
void destroy_engine(kvdb::contracts::IEngine* ptr) {
    delete ptr;
}

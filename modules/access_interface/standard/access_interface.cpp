#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <atomic>
#include <mutex>

#include "i_modules.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

using tcp = net::ip::tcp;

namespace kvdb::modules::access_interface::standard {
    namespace {
        std::string trim(const std::string& value) {
            const auto isNotSpace = [](unsigned char ch) {
                return !std::isspace(ch);
            };

            const auto beginIt = std::find_if(
                value.begin(),
                value.end(),
                isNotSpace
            );

            if (beginIt == value.end()) {
                return {};
            }

            const auto endIt = std::find_if(
                value.rbegin(),
                value.rend(),
                isNotSpace
            ).base();

            return std::string(beginIt, endIt);
        }

        std::string toLower(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                }
            );

            return value;
        }

        bool isExitCommand(const std::string& rawQuery) {
            const std::string normalized = toLower(trim(rawQuery));

            return normalized == "exit"
                || normalized == "quit";
        }

        void writeText(
            websocket::stream<tcp::socket>& ws,
            const std::string& response
        ) {
            beast::error_code ec;

            ws.write(net::buffer(response), ec);

            if (ec) {
                throw beast::system_error(ec);
            }
        }

        std::string buildResponseForRawQuery(
            const std::string& rawQuery,
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor
        ) {
            using namespace kvdb::contracts;

            CmdParseResult parseResult = queryParser.parse(rawQuery);

            if (std::holds_alternative<CmdParseErr>(parseResult)) {
                const CmdParseErr& err = std::get<CmdParseErr>(parseResult);

                if (err != nullptr) {
                    return responseConstructor.buildErrResponse(*err);
                }

                const ParserReturnedNullErr fallbackErr;
                return responseConstructor.buildErrResponse(fallbackErr);
            }

            CmdParseSuccess cmd = std::move(
                std::get<CmdParseSuccess>(parseResult)
            );

            if (cmd == nullptr) {
                const ParserReturnedNullCmdErr err;
                return responseConstructor.buildErrResponse(err);
            }

            CmdExecResult execResult = engine.execute(*cmd);

            if (std::holds_alternative<CmdExecErr>(execResult)) {
                const CmdExecErr& err = std::get<CmdExecErr>(execResult);

                if (err != nullptr) {
                    return responseConstructor.buildErrResponse(*err);
                }

                const EngineReturnedNullErr fallbackErr;
                return responseConstructor.buildErrResponse(fallbackErr);
            }

            const SuccessCmdExecResult& success =
                std::get<SuccessCmdExecResult>(execResult);

            return responseConstructor.buildSuccessResponse(success);
        }
    }

    class StandardAccessInterface final : public kvdb::contracts::IAccessInterface
    {
    public:
        void start(
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor
        ) override {
            stopRequested_.store(false);

            constexpr unsigned short port = 9002;
            const auto address = net::ip::make_address("0.0.0.0");

            net::io_context ioContext{1};
            tcp::acceptor acceptor{ioContext};

            setActiveAcceptor(&acceptor);

            try {
                beast::error_code ec;

                acceptor.open(address.is_v6() ? tcp::v6() : tcp::v4(), ec);

                if (ec) {
                    throw beast::system_error(ec);
                }

                acceptor.set_option(net::socket_base::reuse_address(true), ec);

                if (ec) {
                    throw beast::system_error(ec);
                }

                acceptor.bind(tcp::endpoint{address, port}, ec);

                if (ec) {
                    throw beast::system_error(ec);
                }

                acceptor.listen(net::socket_base::max_listen_connections, ec);

                if (ec) {
                    throw beast::system_error(ec);
                }

                std::cout << "WebSocket access interface started on 0.0.0.0:"
                    << port
                    << '\n';

                while (!stopRequested_.load()) {
                    tcp::socket socket{ioContext};

                    acceptor.accept(socket, ec);

                    if (stopRequested_.load()) {
                        break;
                    }

                    if (ec == net::error::operation_aborted) {
                        break;
                    }

                    if (ec) {
                        std::cerr << "Accept failed: "
                            << ec.message()
                            << '\n';
                        continue;
                    }

                    try {
                        handleSingleConnection(
                            std::move(socket),
                            queryParser,
                            engine,
                            responseConstructor
                        );
                    }
                    catch (const std::exception& ex) {
                        if (stopRequested_.load()) {
                            break;
                        }

                        std::cerr << "Connection handling failed: "
                            << ex.what()
                            << '\n';
                    }
                }

                std::cout << "WebSocket access interface stopped.\n";
                clearActiveAcceptor(&acceptor);
            }
            catch (...) {
                clearActiveAcceptor(&acceptor);
                throw;
            }
        }

        void requestStop() override {
            stopRequested_.store(true);

            std::lock_guard lock(stateMutex_);

            if (activeAcceptor_ != nullptr) {
                beast::error_code ec;
                activeAcceptor_->cancel(ec);
                activeAcceptor_->close(ec);
            }

            if (activeWebSocket_ != nullptr) {
                beast::error_code ec;

                activeWebSocket_->next_layer().cancel(ec);
                activeWebSocket_->next_layer().close(ec);
            }
        }

    private:
        std::atomic_bool stopRequested_{false};
        std::mutex stateMutex_;

        tcp::acceptor* activeAcceptor_ = nullptr;
        websocket::stream<tcp::socket>* activeWebSocket_ = nullptr;

        void setActiveAcceptor(tcp::acceptor* acceptor) {
            std::lock_guard lock(stateMutex_);
            activeAcceptor_ = acceptor;
        }

        void clearActiveAcceptor(tcp::acceptor* acceptor) {
            std::lock_guard lock(stateMutex_);

            if (activeAcceptor_ == acceptor) {
                activeAcceptor_ = nullptr;
            }
        }

        void setActiveWebSocket(websocket::stream<tcp::socket>* ws) {
            std::lock_guard lock(stateMutex_);
            activeWebSocket_ = ws;
        }

        void clearActiveWebSocket(websocket::stream<tcp::socket>* ws) {
            std::lock_guard lock(stateMutex_);

            if (activeWebSocket_ == ws) {
                activeWebSocket_ = nullptr;
            }
        }

        void handleSingleConnection(
            tcp::socket socket,
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor
        ) {
            beast::error_code ec;
            websocket::stream<tcp::socket> ws{std::move(socket)};

            setActiveWebSocket(&ws);

            try {
                ws.set_option(
                    websocket::stream_base::timeout::suggested(
                        beast::role_type::server
                    )
                );

                ws.accept(ec);

                if (stopRequested_.load()) {
                    clearActiveWebSocket(&ws);
                    return;
                }

                if (ec) {
                    throw beast::system_error(ec);
                }

                ws.text(true);

                writeText(
                    ws,
                    responseConstructor.buildSessionStartedResponse()
                );

                beast::flat_buffer buffer;

                while (!stopRequested_.load()) {
                    buffer.consume(buffer.size());

                    ws.read(buffer, ec);

                    if (stopRequested_.load()) {
                        break;
                    }

                    if (ec == websocket::error::closed) {
                        break;
                    }

                    if (ec == net::error::operation_aborted) {
                        break;
                    }

                    if (ec) {
                        throw beast::system_error(ec);
                    }

                    const std::string rawQuery =
                        beast::buffers_to_string(buffer.data());

                    if (isExitCommand(rawQuery)) {
                        writeText(
                            ws,
                            responseConstructor.buildSessionEndedResponse()
                        );

                        ws.close(websocket::close_code::normal, ec);

                        if (ec && ec != websocket::error::closed) {
                            throw beast::system_error(ec);
                        }

                        break;
                    }

                    const std::string response = buildResponseForRawQuery(
                        rawQuery,
                        queryParser,
                        engine,
                        responseConstructor
                    );

                    writeText(ws, response);
                }

                clearActiveWebSocket(&ws);
            }
            catch (...) {
                clearActiveWebSocket(&ws);
                throw;
            }
        }
    };
}

extern "C" __declspec(dllexport)
kvdb::contracts::IAccessInterface* create_access_interface() {
    return new kvdb::modules::access_interface::standard::StandardAccessInterface();
}

extern "C" __declspec(dllexport)
void destroy_access_interface(kvdb::contracts::IAccessInterface* ptr) {
    delete ptr;
}

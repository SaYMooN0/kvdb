#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <iostream>
#include <string>
#include <utility>

#include "i_modules.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace kvdb::modules::access_interface::standard {

    class StandardAccessInterface final : public kvdb::contracts::IAccessInterface
    {
    public:
        void start(
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor) override
        {
            constexpr unsigned short port = 9002;
            const auto address = net::ip::make_address("0.0.0.0");

            net::io_context ioContext{1};
            tcp::acceptor acceptor{ioContext};

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

            std::cout << "WebSocket access interface started on 0.0.0.0:" << port << '\n';

            while (true) {
                tcp::socket socket{ioContext};

                acceptor.accept(socket, ec);
                if (ec) {
                    std::cerr << "Accept failed: " << ec.message() << '\n';
                    continue;
                }

                try {
                    handleSingleConnection(
                        std::move(socket),
                        queryParser,
                        engine,
                        responseConstructor);
                }
                catch (const std::exception& ex) {
                    std::cerr << "Connection handling failed: " << ex.what() << '\n';
                }
            }
        }

    private:
        static void handleSingleConnection(
            tcp::socket socket,
            kvdb::contracts::IQueryParser& queryParser,
            kvdb::contracts::IEngine& engine,
            kvdb::contracts::IResponseConstructor& responseConstructor)
        {
            beast::error_code ec;
            websocket::stream<tcp::socket> ws{std::move(socket)};

            ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
            ws.accept(ec);
            if (ec) {
                throw beast::system_error(ec);
            }

            ws.text(true);

            {
                const std::string connectionEstablishedResponse =
                    responseConstructor.buildSessionStartedResponse();

                ws.write(net::buffer(connectionEstablishedResponse), ec);
                if (ec) {
                    throw beast::system_error(ec);
                }
            }

            beast::flat_buffer buffer;

            while (true) {
                buffer.consume(buffer.size());

                ws.read(buffer, ec);

                if (ec == websocket::error::closed) {
                    break;
                }

                if (ec) {
                    throw beast::system_error(ec);
                }

                const std::string rawQuery = beast::buffers_to_string(buffer.data());
                const std::string parsedQuery = queryParser.parse(rawQuery);

                if (parsedQuery == "exit") {
                    const std::string connectionClosedResponse =
                        responseConstructor.buildSessionEndedResponse();

                    ws.write(net::buffer(connectionClosedResponse), ec);
                    if (ec) {
                        throw beast::system_error(ec);
                    }

                    ws.close(websocket::close_code::normal, ec);
                    if (ec && ec != websocket::error::closed) {
                        throw beast::system_error(ec);
                    }

                    break;
                }

                const std::string engineResult = engine.execute(parsedQuery);
                const std::string response = responseConstructor.buildResponse(engineResult);

                ws.write(net::buffer(response), ec);
                if (ec) {
                    throw beast::system_error(ec);
                }
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
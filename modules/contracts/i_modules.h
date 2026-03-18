#pragma once

#include <string>

namespace kvdb::contracts {
    class IEngine
    {
    public:
        virtual ~IEngine() = default;
        virtual std::string execute(const std::string& parsedQuery) = 0;
    };

    class IQueryParser
    {
    public:
        virtual ~IQueryParser() = default;
        virtual std::string parse(const std::string& rawQuery) = 0;
    };

    class IResponseConstructor
    {
    public:
        virtual ~IResponseConstructor() = default;
        virtual std::string buildResponse(const std::string& engineResult) = 0;
    };

    class IServer
    {
    public:
        virtual ~IServer() = default;

        virtual void start(
            IQueryParser& queryParser,
            IEngine& engine,
            IResponseConstructor& responseConstructor) = 0;
    };
}
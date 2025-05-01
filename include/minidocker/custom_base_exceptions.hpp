#ifndef MINIDOCKER_CUSTOM_BASE_EXCEPTIONS_H
#define MINIDOCKER_CUSTOM_BASE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace minidocker
{
    class CLIParserException : public std::runtime_error {
    public:
        explicit CLIParserException(const std::string& message)
            : std::runtime_error(message) {}
    };

    class ContainerRuntimeException : public std::runtime_error {
    public:
        explicit ContainerRuntimeException(const std::string& message)
            : std::runtime_error(message) {}
    };

}

#endif
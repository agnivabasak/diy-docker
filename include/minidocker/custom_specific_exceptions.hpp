#ifndef MINIDOCKER_CUSTOM_SPECIFIC_EXCEPTIONS_H
#define MINIDOCKER_CUSTOM_SPECIFIC_EXCEPTIONS_H

#include "custom_base_exceptions.hpp"

namespace minidocker
{
    class UserMapException: ContainerRuntimeException {
    public:
        explicit UserMapException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };

    class CgroupLimitException : ContainerRuntimeException {
    public:
        explicit CgroupLimitException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };

    class HostnameException : ContainerRuntimeException {
    public:
        explicit HostnameException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };

    class MountException : ContainerRuntimeException {
    public:
        explicit MountException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };

    class UnmountException : ContainerRuntimeException {
    public:
        explicit UnmountException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };

    class CleanupCgroupException : ContainerRuntimeException {
    public:
        explicit CleanupCgroupException(const std::string& message)
            : ContainerRuntimeException(message) {}
    };
}

#endif
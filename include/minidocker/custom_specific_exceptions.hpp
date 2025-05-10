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

    class ImageManifestException : ImageException {
    public:
        explicit ImageManifestException(const std::string& message)
            : ImageException(message) {}
    };

    class ImageConfigException : ImageException {
    public:
        explicit ImageConfigException(const std::string& message)
            : ImageException(message) {}
    };

    class ImageTarballException : ImageException {
    public:
        explicit ImageTarballException(const std::string& message)
            : ImageException(message) {}
    };

    class ImageExtractionException : ImageException {
    public:
        explicit ImageExtractionException(const std::string& message)
            : ImageException(message) {}
    };
}

#endif
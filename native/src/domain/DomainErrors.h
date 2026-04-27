#pragma once

#include <stdexcept>
#include <string>

namespace mfinlogs::domain {

class DomainError final : public std::runtime_error {
public:
    explicit DomainError(const std::string& message)
        : std::runtime_error(message) {}
};

class NotImplementedError final : public std::logic_error {
public:
    explicit NotImplementedError(const std::string& feature)
        : std::logic_error(feature + " is not implemented in the native rewrite yet") {}
};

} // namespace mfinlogs::domain


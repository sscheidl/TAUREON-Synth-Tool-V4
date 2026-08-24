#pragma once

#include "MidiError.hpp"

#include <optional>
#include <utility>
#include <variant>

namespace taureon::midi {

template <typename T>
class Result {
public:
    static Result success(T value) { return Result(std::move(value)); }
    static Result failure(MidiError error) { return Result(std::move(error)); }

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T& value() { return std::get<T>(storage_); }
    [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
    [[nodiscard]] MidiError& error() { return std::get<MidiError>(storage_); }
    [[nodiscard]] const MidiError& error() const { return std::get<MidiError>(storage_); }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(MidiError error) : storage_(std::move(error)) {}

    std::variant<T, MidiError> storage_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(); }
    static Result failure(MidiError error) { return Result(std::move(error)); }

    [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }
    [[nodiscard]] MidiError& error() { return error_.value(); }
    [[nodiscard]] const MidiError& error() const { return error_.value(); }

private:
    Result() = default;
    explicit Result(MidiError error) : error_(std::move(error)) {}

    std::optional<MidiError> error_;
};

} // namespace taureon::midi

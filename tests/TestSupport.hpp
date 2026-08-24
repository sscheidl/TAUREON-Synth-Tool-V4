#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

namespace taureon::test {

inline void require(const bool condition, const char* expression, const char* file, const int line) {
    if (condition) return;
    throw std::runtime_error(std::string(file) + ':' + std::to_string(line) +
                             " requirement failed: " + expression);
}

template <typename Function>
int run(Function&& function) {
    try {
        function();
        std::cout << "PASS" << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << std::endl;
        return 1;
    }
}

} // namespace taureon::test

#define TAUREON_REQUIRE(expression) \
    ::taureon::test::require(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

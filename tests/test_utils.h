#ifndef CACHE_ADAPTIVE_TEST_UTILS_H
#define CACHE_ADAPTIVE_TEST_UTILS_H

#include <iostream>
#include <stdexcept>
#include <string>

namespace test_utils {

inline void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failure: " << message << std::endl;
        throw std::runtime_error("test failure: " + message);
    }
}

}  // namespace test_utils

#endif

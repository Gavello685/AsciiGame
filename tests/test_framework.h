#pragma once

// Minimal test harness. The project has no external dependencies beyond SDL2,
// so tests use a self-registering TEST macro rather than a framework.
//
//   TEST(chunk_coords_negative) {
//       CHECK_EQ(World::world_to_chunk_x(-1), -1);
//   }

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

// Failures accumulate so one test reports every problem it finds.
inline std::vector<std::string>& current_failures() {
    static std::vector<std::string> failures;
    return failures;
}

inline void fail(const std::string& where, const std::string& what) {
    current_failures().push_back(where + ": " + what);
}

template <typename T>
std::string describe(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string describe(bool value) { return value ? "true" : "false"; }

int run_all();

}

#define TEST(name)                                                         \
    static void test_##name();                                             \
    static ::testing::Registrar registrar_##name(#name, test_##name);      \
    static void test_##name()

#define TEST_LOCATION (std::string(__FILE__) + ":" + std::to_string(__LINE__))

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) ::testing::fail(TEST_LOCATION, "CHECK(" #cond ") failed"); \
    } while (0)

#define CHECK_EQ(actual, expected)                                         \
    do {                                                                   \
        auto actual_value = (actual);                                      \
        auto expected_value = (expected);                                  \
        if (!(actual_value == expected_value)) {                           \
            ::testing::fail(TEST_LOCATION,                                 \
                            std::string(#actual) + " == " + #expected +    \
                            " failed (got " +                              \
                            ::testing::describe(actual_value) +            \
                            ", want " +                                    \
                            ::testing::describe(expected_value) + ")");    \
        }                                                                  \
    } while (0)

#define CHECK_NE(actual, unexpected)                                       \
    do {                                                                   \
        auto actual_value = (actual);                                      \
        auto unexpected_value = (unexpected);                              \
        if (actual_value == unexpected_value) {                            \
            ::testing::fail(TEST_LOCATION,                                 \
                            std::string(#actual) + " != " + #unexpected +  \
                            " failed (both " +                             \
                            ::testing::describe(actual_value) + ")");      \
        }                                                                  \
    } while (0)

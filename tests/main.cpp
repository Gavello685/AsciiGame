#include "test_framework.h"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace testing {

int run_all() {
    int failed = 0;
    for (const auto& test : registry()) {
        current_failures().clear();
        test.body();
        if (current_failures().empty()) {
            std::cout << "  PASS  " << test.name << "\n";
        } else {
            failed++;
            std::cout << "  FAIL  " << test.name << "\n";
            for (const auto& failure : current_failures()) {
                std::cout << "          " << failure << "\n";
            }
        }
    }

    std::cout << "\n"
              << registry().size() - static_cast<size_t>(failed) << " passed, "
              << failed << " failed, "
              << registry().size() << " total\n";
    return failed == 0 ? 0 : 1;
}

}

int main() {
    // Point saves and settings at a scratch directory so tests never touch a
    // real player's data.
    std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "ascii_game_tests";
    std::error_code ec;
    std::filesystem::remove_all(scratch, ec);
    std::filesystem::create_directories(scratch, ec);

#ifdef _WIN32
    _putenv_s("ASCII_GAME_DATA_DIR", scratch.string().c_str());
#else
    setenv("ASCII_GAME_DATA_DIR", scratch.string().c_str(), 1);
#endif

    std::cout << "Running " << testing::registry().size() << " tests\n";
    int result = testing::run_all();

    std::filesystem::remove_all(scratch, ec);
    return result;
}

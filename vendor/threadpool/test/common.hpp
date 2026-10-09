#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstring>

enum class Tier { Fast, Normal, Thorough };

inline Tier parse_tier(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tier") == 0 && i + 1 < argc) {
            std::string val = argv[i + 1];
            if (val == "fast") return Tier::Fast;
            if (val == "normal") return Tier::Normal;
            if (val == "thorough") return Tier::Thorough;
        }
    }
    return Tier::Normal; // default
}

inline const char* tier_name(Tier t) {
    switch (t) {
        case Tier::Fast: return "fast";
        case Tier::Normal: return "normal";
        case Tier::Thorough: return "thorough";
    }
    return "unknown";
}

// ---- RAII timer ----
class Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;
    clock::duration* out;
public:
    Timer(clock::duration* out) : start(clock::now()), out(out) {}
    ~Timer() { if (out) *out = clock::now() - start; }
};

// ---- Formatted output helpers ----
inline std::string dur_str(std::chrono::nanoseconds d) {
    double us = std::chrono::duration<double, std::micro>(d).count();
    if (us < 1000.0) return std::to_string(static_cast<int>(us)) + " us";
    double ms = us / 1000.0;
    if (ms < 1000.0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << ms << " ms";
        return ss.str();
    }
    double s = ms / 1000.0;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3) << s << " s";
    return ss.str();
}

inline std::string pct_str(double val) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (val * 100.0) << "%";
    return ss.str();
}

// Separator line
inline void print_sep(const char* title = nullptr) {
    if (title) std::cout << "\n==== " << title << " ====\n";
    else std::cout << "----------------------------------------\n";
}

// ---- Test result struct ----
struct TestResult {
    std::string name;
    std::string value;
    std::string unit;
};

inline void print_result(const TestResult& r) {
    std::cout << "  " << std::left << std::setw(36) << r.name
              << std::right << std::setw(12) << r.value
              << "  " << r.unit << "\n";
}

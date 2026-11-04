#pragma once

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

// Display helpers shared by the terminal interface and the benchmark report.

// 150000 -> "150,000"
inline std::string formatCount(std::size_t value) {
    std::string digits = std::to_string(value);
    for (std::size_t i = digits.size(); i > 3; i -= 3) {
        digits.insert(i - 3, ",");
    }
    return digits;
}

inline std::string formatFixed(double value, int decimals) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

// 1 MiB = 1,048,576 bytes.
inline std::string formatMiB(std::size_t bytes) {
    return formatFixed(static_cast<double>(bytes) / (1024.0 * 1024.0), 2) + " MiB";
}

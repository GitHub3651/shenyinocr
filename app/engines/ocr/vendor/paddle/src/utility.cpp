#include "engines/ocr/vendor/paddle/include/utility.h"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace PaddleOCR {

std::vector<std::string> Utility::ReadDict(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to open OCR text file: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::vector<std::vector<std::vector<int>>> Utility::SortQuadBoxes(
        const std::vector<std::vector<std::vector<int>>> &boxes)
{
    std::vector<std::vector<std::vector<int>>> sorted = boxes;
    std::sort(
                sorted.begin(),
                sorted.end(),
                [](const std::vector<std::vector<int>> &left,
                   const std::vector<std::vector<int>> &right) {
        return left[0][1] < right[0][1]
                || (left[0][1] == right[0][1]
                    && left[0][0] < right[0][0]);
    });

    if (sorted.size() < 2) {
        return sorted;
    }
    for (size_t first = 0; first + 1 < sorted.size(); ++first) {
        for (size_t current = first + 1; current > 0; --current) {
            if (std::abs(sorted[current][0][1]
                         - sorted[current - 1][0][1]) < 10
                    && sorted[current][0][0]
                    < sorted[current - 1][0][0]) {
                std::swap(sorted[current], sorted[current - 1]);
            } else {
                break;
            }
        }
    }
    return sorted;
}

} // namespace PaddleOCR

#pragma once

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace PaddleOCR {

class Utility
{
public:
    static std::vector<std::string> ReadDict(const std::string &path);

    static std::vector<std::vector<std::vector<int>>> SortQuadBoxes(
            const std::vector<std::vector<std::vector<int>>> &boxes);

    template <class ForwardIterator>
    static size_t Argmax(ForwardIterator first, ForwardIterator last)
    {
        return static_cast<size_t>(
                    std::distance(first, std::max_element(first, last)));
    }
};

} // namespace PaddleOCR

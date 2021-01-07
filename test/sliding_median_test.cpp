#include "../src/sliding_median.h"

#include <iostream>

int main()
{
    std::vector<int> data { 1, 3, -1, -3, 5, 3, 6, 8 };
    std::vector<int> expected { 1, 2, 1, -1, -1, 3, 5, 6 };

    SlidingMedian<int, long, long> slide(3);

    long t = 0;
    for (int v : data) {
        auto sample = std::make_pair(v, t);
        if (expected[t++] != slide.offer(sample)) {
            std::cout << "Error" << std::endl;
        }
    }

    return 0;
}

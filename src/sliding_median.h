#include <queue>
#include <set>

template <class T, class Timestamp, class Duration> class SlidingMedian {
    using Sample = std::pair<T, Timestamp>;

public:
    SlidingMedian(Duration size)
        : window_size(size)
    {
    }

    T offer(const Sample& sample)
    {
        // Assume that equal timestamps correspond to equal values.
        // Remove oldest values from the old data.
        Timestamp oldest = sample.second - window_size;
        while (!data.empty() && data.front().second <= oldest) {
            Sample& s = data.front();
            if (left.erase(s) == 0) {
                right.erase(s);
            }
            data.pop();
        }

        // Insert the new value into the correct tree
        data.push(sample);
        if (!right.empty() && sample.first < right.begin()->first) {
            left.insert(sample);
        } else {
            right.insert(sample);
        }

        // Rebalance: we could have deleted enough values to disturb the balance
        while (left.size() > right.size()) {
            auto it = left.end();
            --it;
            right.insert(*it);
            left.erase(it);
        }
        while (left.size() + 1 < right.size()) {
            auto it = right.begin();
            left.insert(*it);
            right.erase(it);
        }

        // Return the new median value.
        T middle = right.begin()->first;
        if (left.size() == right.size()) {
            return (left.rbegin()->first + middle) / 2;
        }

        // left.size() < right.size()
        return middle;
    }

    void set_window_size(Duration size) { window_size = size; }

private:
    Duration window_size;

    // Sorted by timestamp
    std::queue<Sample> data;

    // Sorted by value
    std::set<Sample> left;
    std::set<Sample> right;
};

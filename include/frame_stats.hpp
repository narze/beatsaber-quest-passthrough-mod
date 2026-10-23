#pragma once

// Frame interval statistics for the frame timing log. No game dependencies,
// so it is unit tested on the host. Not thread-safe: use from the render
// thread only.

#include <algorithm>
#include <vector>

namespace passthrough {

class FrameStats {
public:
  void Add(double intervalMs) { intervals_.push_back(intervalMs); }
  void Reset() { intervals_.clear(); }

  size_t Frames() const { return intervals_.size(); }

  double AverageMs() const {
    if (intervals_.empty()) return 0.0;
    double sum = 0.0;
    for (double v : intervals_) sum += v;
    return sum / static_cast<double>(intervals_.size());
  }

  double MinMs() const {
    return intervals_.empty() ? 0.0 : *std::min_element(intervals_.begin(), intervals_.end());
  }

  double WorstMs() const {
    return intervals_.empty() ? 0.0 : *std::max_element(intervals_.begin(), intervals_.end());
  }

  // Frames that took more than 1.5 x the target period (a missed refresh).
  size_t SlowFrames(double targetMs) const {
    return static_cast<size_t>(
        std::count_if(intervals_.begin(), intervals_.end(), [&](double v) { return v > targetMs * 1.5; }));
  }

private:
  std::vector<double> intervals_;
};

} // namespace passthrough

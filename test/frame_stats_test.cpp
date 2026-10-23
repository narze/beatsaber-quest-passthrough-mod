// Host unit tests for FrameStats. Run: ./test/run.sh
#include "frame_stats.hpp"

#include <cassert>
#include <cstdio>

using namespace passthrough;

static void empty_has_no_frames() {
  FrameStats s;
  assert(s.Frames() == 0);
  assert(s.AverageMs() == 0.0);
  assert(s.MinMs() == 0.0);
}

static void average_worst_and_slow_frames() {
  FrameStats s;
  for (int i = 0; i < 8; i++) s.Add(11.1);
  s.Add(25.0); // a dropped frame
  s.Add(11.1);
  assert(s.Frames() == 10);
  assert(s.WorstMs() == 25.0);
  assert(s.SlowFrames(11.1) == 1); // slow: more than 1.5 x the target period
  assert(s.MinMs() == 11.1);        // display times step in whole periods
  double avg = s.AverageMs();
  assert(avg > 12.4 && avg < 12.6);
}

static void reset_clears() {
  FrameStats s;
  s.Add(30.0);
  s.Reset();
  assert(s.Frames() == 0 && s.WorstMs() == 0.0 && s.SlowFrames(11.1) == 0);
}

int main() {
  empty_has_no_frames();
  average_worst_and_slow_frames();
  reset_clears();
  std::puts("all frame_stats tests passed");
}

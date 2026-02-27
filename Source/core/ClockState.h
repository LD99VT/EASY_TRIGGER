#pragma once

#include "Timecode.h"

#include <optional>
#include <mutex>
#include <tuple>

namespace bridge::core
{
struct TimecodeWithFps
{
    Timecode tc;
    int fpsType {};
};

class ClockState
{
public:
    explicit ClockState (int defaultFpsType = FRAME_RATE_25);

    void reset();
    bool isValid() const;
    std::tuple<bool, Timecode, int, double> syncInfo() const;
    void update (int hours, int minutes, int seconds, int frames, int fpsType, double inputLatencySeconds = 0.0, double nowTs = -1.0);
    std::optional<TimecodeWithFps> nowTc (double nowTs = -1.0) const;

private:
    mutable std::mutex mutex_;
    bool valid_ { false };
    Timecode tc_ { 0, 0, 0, 0 };
    int fpsType_ { FRAME_RATE_25 };
    double syncTs_ { 0.0 };
};
} // namespace bridge::core


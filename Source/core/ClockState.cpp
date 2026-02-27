#include "ClockState.h"

#include <algorithm>
#include <cmath>
#include <chrono>

namespace bridge::core
{
namespace
{
double monotonicSeconds()
{
    using namespace std::chrono;
    return duration<double> (steady_clock::now().time_since_epoch()).count();
}
}

ClockState::ClockState (int defaultFpsType)
    : fpsType_ (defaultFpsType)
{
}

void ClockState::reset()
{
    std::scoped_lock lock (mutex_);
    valid_ = false;
}

bool ClockState::isValid() const
{
    std::scoped_lock lock (mutex_);
    return valid_;
}

std::tuple<bool, Timecode, int, double> ClockState::syncInfo() const
{
    std::scoped_lock lock (mutex_);
    return { valid_, tc_, fpsType_, syncTs_ };
}

void ClockState::update (int hours, int minutes, int seconds, int frames, int fpsType, double inputLatencySeconds, double nowTs)
{
    if (nowTs < 0.0)
        nowTs = monotonicSeconds();

    const double latency = std::max (0.0, inputLatencySeconds);
    Timecode tc {
        ((hours % 24) + 24) % 24,
        ((minutes % 60) + 60) % 60,
        ((seconds % 60) + 60) % 60,
        frames
    };

    std::scoped_lock lock (mutex_);
    tc_ = tc;
    fpsType_ = fpsType;
    syncTs_ = nowTs - latency;
    valid_ = true;
}

std::optional<TimecodeWithFps> ClockState::nowTc (double nowTs) const
{
    if (nowTs < 0.0)
        nowTs = monotonicSeconds();

    Timecode tc;
    int fpsType = FRAME_RATE_25;
    double syncTs = 0.0;
    {
        std::scoped_lock lock (mutex_);
        if (! valid_)
            return std::nullopt;
        tc = tc_;
        fpsType = fpsType_;
        syncTs = syncTs_;
    }

    double fps = fpsTypeToValue (fpsType);
    if (fps <= 0.0)
        fps = 25.0;

    const int elapsedFrames = static_cast<int> (std::floor (std::max (0.0, nowTs - syncTs) * fps));
    Timecode now = applyFrameOffset (tc.hours, tc.minutes, tc.seconds, tc.frames, fpsType, elapsedFrames);
    return TimecodeWithFps { now, fpsType };
}
} // namespace bridge::core


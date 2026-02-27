#include "Timecode.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace bridge::core
{
namespace
{
constexpr int kDfDayFrames = 24 * 3600 * 30 - 2 * 24 * 54; // 2589408
}

int normalizeFpsTypeLegacy4 (int fpsType)
{
    switch (fpsType)
    {
        case FRAME_RATE_24:
        case FRAME_RATE_25:
        case FRAME_RATE_2997_DF:
        case FRAME_RATE_30:
            return fpsType;
        case FRAME_RATE_23976:
            return FRAME_RATE_24;
        case FRAME_RATE_2997_ND:
            return FRAME_RATE_30;
        default:
            return FRAME_RATE_25;
    }
}

int fpsNominalFrames (int fpsType)
{
    const int t = normalizeFpsTypeLegacy4 (fpsType);
    if (t == FRAME_RATE_24)
        return 24;
    if (t == FRAME_RATE_25)
        return 25;
    return 30;
}

double fpsTypeToValue (int fpsType)
{
    const int t = normalizeFpsTypeLegacy4 (fpsType);
    if (t == FRAME_RATE_24)
        return 24.0;
    if (t == FRAME_RATE_25)
        return 25.0;
    if (t == FRAME_RATE_2997_DF)
        return 29.97;
    return 30.0;
}

int fpsTypeToMtcArtnetCode (int fpsType)
{
    const int t = normalizeFpsTypeLegacy4 (fpsType);
    if (t == FRAME_RATE_24) return FRAME_RATE_24;
    if (t == FRAME_RATE_25) return FRAME_RATE_25;
    if (t == FRAME_RATE_2997_DF) return FRAME_RATE_2997_DF;
    return FRAME_RATE_30;
}

int timecodeToDfFrames (int h, int m, int s, int f)
{
    h = (h % 24 + 24) % 24;
    m = (m % 60 + 60) % 60;
    s = (s % 60 + 60) % 60;
    f = (f % 30 + 30) % 30;

    int total = ((h * 3600 + m * 60 + s) * 30) + f;
    const int totalMinutes = h * 60 + m;
    const int dropped = 2 * (totalMinutes - (totalMinutes / 10));
    total -= dropped;
    return total;
}

Timecode dfFramesToTimecode (int total)
{
    total %= kDfDayFrames;
    if (total < 0)
        total += kDfDayFrames;

    constexpr int framesPer10m = 17982;
    constexpr int framesMin0 = 1800;
    constexpr int framesMinDf = 1798;

    const int tenMinBlocks = total / framesPer10m;
    const int rem = total % framesPer10m;

    int minInBlock = 0;
    int tcIndex = 0;
    if (rem < framesMin0)
    {
        minInBlock = 0;
        tcIndex = rem;
    }
    else
    {
        const int rem2 = rem - framesMin0;
        minInBlock = 1 + (rem2 / framesMinDf);
        const int frameInMin = rem2 % framesMinDf;
        tcIndex = frameInMin + 2;
    }

    const int totalMinutes = tenMinBlocks * 10 + minInBlock;
    Timecode tc;
    tc.hours = (totalMinutes / 60) % 24;
    tc.minutes = totalMinutes % 60;
    tc.seconds = tcIndex / 30;
    tc.frames = tcIndex % 30;
    return tc;
}

std::string framesToTimecodeString (double framesFloat, double fps)
{
    const int fpsInt = std::max (1, static_cast<int> (std::lround (fps)));
    int totalFrames = static_cast<int> (std::lround (framesFloat));
    totalFrames = std::max (0, totalFrames);

    const int f = totalFrames % fpsInt;
    const int restSec = totalFrames / fpsInt;
    const int s = restSec % 60;
    const int m = (restSec / 60) % 60;
    const int h = restSec / 3600;

    std::array<char, 16> buffer {};
    std::snprintf (buffer.data(), buffer.size(), "%02d:%02d:%02d:%02d", h, m, s, f);
    return std::string (buffer.data());
}

std::optional<int> timecodeStringToTotalFrames (const std::string& tc, double fps)
{
    const int fpsInt = std::max (1, static_cast<int> (std::lround (fps)));
    if (tc.empty())
        return std::nullopt;

    std::string txt = tc;
    int sign = 1;
    if (txt.front() == '-')
    {
        sign = -1;
        txt.erase (txt.begin());
    }

    int h = 0, m = 0, s = 0, f = 0;
    if (std::sscanf (txt.c_str(), "%d:%d:%d:%d", &h, &m, &s, &f) != 4)
        return std::nullopt;

    if (h < 0 || m < 0 || s < 0 || f < 0 || m >= 60 || s >= 60 || f >= fpsInt)
        return std::nullopt;

    const int total = ((h * 3600 + m * 60 + s) * fpsInt) + f;
    return sign * total;
}

Timecode applyFrameOffset (int h, int m, int s, int f, int fpsType, int offset)
{
    const int fpsT = normalizeFpsTypeLegacy4 (fpsType);
    if (fpsT == FRAME_RATE_2997_DF)
        return dfFramesToTimecode (timecodeToDfFrames (h, m, s, f) + offset);

    const int fpsInt = fpsNominalFrames (fpsT);
    const int dayFrames = 24 * 3600 * fpsInt;
    int total = ((h * 3600 + m * 60 + s) * fpsInt + f) + offset;
    total %= dayFrames;
    if (total < 0)
        total += dayFrames;

    Timecode tc;
    tc.hours = total / (3600 * fpsInt);
    total %= (3600 * fpsInt);
    tc.minutes = total / (60 * fpsInt);
    total %= (60 * fpsInt);
    tc.seconds = total / fpsInt;
    tc.frames = total % fpsInt;
    return tc;
}
} // namespace bridge::core


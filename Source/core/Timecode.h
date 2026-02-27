#pragma once

#include <string>
#include <optional>

namespace bridge::core
{
enum FrameRateType : int
{
    FRAME_RATE_24 = 0,
    FRAME_RATE_25 = 1,
    FRAME_RATE_2997_DF = 2,
    FRAME_RATE_30 = 3,
    FRAME_RATE_23976 = 4,
    FRAME_RATE_2997_ND = 5
};

struct Timecode
{
    int hours {};
    int minutes {};
    int seconds {};
    int frames {};
};

int normalizeFpsTypeLegacy4 (int fpsType);
int fpsNominalFrames (int fpsType);
int fpsTypeToMtcArtnetCode (int fpsType);
int timecodeToDfFrames (int h, int m, int s, int f);
Timecode dfFramesToTimecode (int total);
std::string framesToTimecodeString (double framesFloat, double fps);
std::optional<int> timecodeStringToTotalFrames (const std::string& tc, double fps);
Timecode applyFrameOffset (int h, int m, int s, int f, int fpsType, int offset);
double fpsTypeToValue (int fpsType);
} // namespace bridge::core

